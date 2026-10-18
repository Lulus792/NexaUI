# Initial Linux AT-SPI native evidence

Date: 2026-10-16. This report covers the initial private Linux transport, not
complete Linux accessibility or an Orca acceptance result. The public repository
remains [Lulus792/NexaUI](https://github.com/Lulus792/NexaUI).

Implementation [08a4237](https://github.com/Lulus792/NexaUI/commit/08a4237e111e002deba66c475a8dd18422ada943)
passes the complete [desktop CI run](https://github.com/Lulus792/NexaUI/actions/runs/38913242684):
all six static/shared OS jobs and Linux sanitizers succeed. Optional Windows
ASan and fuzz campaigns were not requested. The rows below distinguish executed
tests from environment-dependent skips.

| CI configuration | Passed | Skipped | Native AT-SPI cases |
| --- | ---: | ---: | --- |
| Ubuntu static, GCC 13.3 | 99 | 7 | All five passed |
| Ubuntu shared, GCC 13.3 | 99 | 7 | All five passed |
| Ubuntu Clang 18.1.3 ASan/UBSan | 99 | 7 | All five passed |
| Windows static/shared, MSVC 19.51.36260 | 98 each | 0 | Not a Linux target |
| macOS static/shared, AppleClang 21 ARM64 | 85 each | 0 | Not a Linux target |

Ubuntu 24.04 uses D-Bus 1.14.10-4ubuntu4.1 and libatspi
2.52.0-1build1. Both real-registry client cases execute in all three Linux jobs:
direct/discovery take 6.03/6.17 s static, 6.14/6.24 s shared and 6.34/6.49 s
under sanitizers. They are not skipped or mock-only checks. The seven Ubuntu
skips require a Wayland session unavailable on that runner. Local WSLg validation
below is separate evidence. Windows UIA and compact NexaCloud UIA pass in both
CI variants; macOS's separate OS AX client also executes and passes in both.
Installed and add_subdirectory C consumers build in all six OS variants, and
installed consumers run successfully.

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
reconnection, bus wakeup and fractional-coordinate checks. The same independent
client also passes on actual WSLg Wayland in all three local configurations.

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
The same production revision passes the native Windows cases on fresh CI hosts;
that does not establish the cause of the local failures.

## Retained CI evidence

Job logs were read for all seven successful jobs. Both Linux artifact ZIPs were
downloaded, hashed and their LastTest.log files inspected. The shared artifact
contains the actual independent-client success output for both real-registry
cases. The static LastTest.log is the later 20-repeat Vulkan lifecycle run, which
overwrites the main suite log; its AT-SPI results come from the main job log.
The executed/skipped counts above use the main CTest output rather than assuming
that every LastTest.log entry was executed successfully.

| Artifact | ID | SHA-256 |
| --- | --- | --- |
| linux-gpu-captures-shared-OFF | 12050838827 | 5bf3d9b8ef99bfc25b7d7cf5b6e9051d0fa52e6625f9aea26eaec0a76be5b901 |
| linux-gpu-captures-shared-ON | 12051840463 | 9f2917ef476a7552e5402791861e2cf7b9d9287b946f5add886eaaf6c414dd1b |

Local copies are `out/atspi-08a4237-{static,shared}.zip`, their selected
`LastTest.log` files and `out/atspi-ci-08a4237-receipt.json`. The hashes match the
published GitHub artifact digests. None is an independent physical/Orca signoff.

## Remaining acceptance

The later [Orca focus follow-up](orca-focus-report.md) fixes missing focused
events for newly published incarnations and records nine actual native-example
control/dialog announcements. It retains long response times and registry
warnings as open issues; this historical transport report does not imply a
human screenreader or complete role/interface acceptance.

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
