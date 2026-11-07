# Native tree keyboard and AT-SPI checks — 7 November 2026

Consumer/test source:
[`b786e8081491ed34417331a9096dd4cbb1438242`](https://github.com/Lulus792/NexaUI/commit/b786e8081491ed34417331a9096dd4cbb1438242).
Installed SDK source: `8dbd3d961e5e7d4a51a40919e15051e31a67a887`.
Base: `15c9355d84618c1cfb0a5b4da206afa66f4c94ca`.
This advances L06/I01/A02/A06/Q04 without completing those whole requirements.

## Native behavior and independent observer

The new [standalone consumer](../../tests/tree_navigation_native/README.md) opens
an ordinary public Wayland window and software presenter on the unchanged pinned
Aetheri test compositor. It shares the existing portable navigation test's small
hierarchy through `tests/tree_navigation_fixture.h`, adding optional borrowed
font layouts for readable labels. The portable assertions remain in place.
This fixture is not an Aetheri Files/component model or child-loading service.

A separate OS libatspi client includes no NexaUI headers and links no NexaUI
code. Its actual link command and `ldd` output are retained. It discovers the
exact host PID and tree-item roles, requests initial/native refocus through
Component, and observes 16 keyboard steps delivered through the private
compositor's acknowledged evdev control channel. The host receives those events
through its real Wayland/XKB path and uses public context, polling, semantic
publication and software APIs only.

Before each native check, the driver waits for a published host frame containing
the entire native key batch. The client independently queries native focus,
expanded and selected states, and requires the matching focused-object event.
Expansion checks require the exact root/child object paths and expected boolean
values. Selection requires both the new child's selected event and the previous
leaf's deselection event. Unrelated host events cannot satisfy those assertions.

The steps cover parent/child navigation, collapse/re-expansion, Home/End, a
clamped Down at the end, independent second-tree focus, explicit Enter selection
and collapse followed by Down in one batch. Navigation alone retains the app's
selection. Child collapse retires the retained leaf reference; parent collapse
retires the retained child and fresh leaf. Re-expansion creates fresh native
paths while restoring the same app-owned selection. This tests selected states,
not the entire AT-SPI Selection interface or complete virtualized selected sets.

## Actual profiles and counterexamples

| Profile | Portable CTest | Native peer | Ordinary host close |
| --- | ---: | --- | --- |
| Static Release | 1/1, no skips | 16 steps, exit 0 | Escape, exit 0 |
| Shared Release | 1/1, no skips | 16 steps, exit 0 | Escape, exit 0 |
| Static Debug ASan/UBSan/LSan | 1/1, no skips | 16 steps, exit 0 | Escape, exit 0 |
| Shared Release, AT-SPI OFF | 1/1, no skips | Expected discovery rejection, exit 77 | Escape, exit 0 |

Each final positive observed 16 focus, eight total expansion and two selection
events. The required root expansion false/true and child expansion false/true
counts were each one; child selection and leaf deselection were each one.
These are observed counts in this fixture, not general event-rate guarantees.
Positive hosts ended with selection 13 and independent-tree selection 31. The
provider-off host remained mapped during discovery rejection and ended with its
unchanged initial selections 15/31. None of these accepted cases required SIGKILL.

The same shared host executable, with only the older shared SDK selected through
`LD_LIBRARY_PATH`, successfully maps and accepts native focus but fails the first
Down step. Both native key events reach a published frame; focus remains 11
instead of becoming 13. The independent peer reports step 0 failure. Its process
maps confirm the old `bae657a4cdaaed6e4f6dc426b71ad17c69586c12` library, SHA-256
`5da1850c8e1a0678a2db9708bf6f8901a322ce2a10197a7cd456cb10f9912966`.
The driver failure and private failure cleanup are retained; an arbitrary setup
error is not counted as this negative. Final and initial peer versions both
detect that specific older-library failure.

The initial peer checked aggregate expansion/selection events. Review strengthened
it to exact paths/values and both selection directions; all three native profiles
were rerun with that peer. Initial positive logs, captures and the earlier peer
binary/source remain retained separately from final acceptance.

## Builds, sources and images

Consumers build as C17 with `-Wall -Wextra -Wpedantic -Werror`. Release uses
`-O3 -DNDEBUG`; the static Debug consumer/SDK use ASan/UBSan and frame pointers,
with leak detection and halt-on-error enabled and no suppressions. The independent
peer is a separate GCC `-O2` C17 build against OS libatspi/GLib/GObject. Private
development-prefix headers/libraries are explicitly supplied; the standalone
CMake client option is disabled for those guest builds. The independent peer is
not claimed to be sanitizer-instrumented.

The three existing installed positive SDK artifacts are reused byte-for-byte.
A separate warning-clean shared AT-SPI-disabled SDK is built from the same frozen
`8dbd3d9` source for the discovery negative. All 159 selected production/build/data
inputs match between that SDK source and the consumer tree; this step changes
tests and their documentation only. The complete 1,022-file consumer source tree
matches its code archive, with explicit CRLF/LF normalization for copied text.
Public API/ABI, version 0.1.23, SDK dependencies and Aetheri's source/pin are unchanged.

Environment: Fedora 44 x86-64, Linux 6.19.10-300.fc44, GCC 16.2.1, CMake 4.3.0,
Python 3.14.3. Compositor SHA-256:
`631ec1740d9de6daac790d2c29351df7ccb2cabce936ac6ab35ba59b8cb71c75`,
from the frozen Aetheri `d56d69523c33a48648af343e7d36526f84518c90` test build.
Liberation Sans input SHA-256:
`76d04c18ea243f426b7de1f3ad208e927008f961dc5945e5aad352d0dfde8ee8`.
Guest/host wall clocks differ; only newly copied source mtimes were normalized.

Each final positive retains two complete captures of all three synthetic outputs:
18 full PPMs. All 18 are byte-identical to their counterparts before the peer-only
assertion strengthening, using unchanged host/SDK binaries. The Release output-0
collapsed/restored images were converted losslessly and inspected: the removed
leaf is absent, its parent has visible focus, and restored selection remains
distinct from root focus. These are supplementary virtual-output images, not
presentation acknowledgements or physical-monitor/human visual acceptance.

The new fixture initially used a nonexistent window-descriptor member, created
a capture directory with 0755 permissions rejected by the unchanged compositor,
and called the descriptor-poll API with an invalid `-1` descriptor. The retained
initial sources/logs show these harness failures. The final host uses the ordinary
public `nx_window_poll`; private directories are 0700. No SDK guard, assertion,
timeout or sanitizer setting was relaxed. Frozen previous evidence was preserved.

## Remaining adoption

Evidence: `D:/NexaUI/out/nexaui-native-tree-20261107.tar.gz`, SHA-256
`75abddd029609e64ef1be3e6a187d04869c81ec68e84fcca991d8193be7edc6d`.
All 1,457 manifest files and six symbolic links were independently verified
after transfer to the local host. Exact sources, SDKs, builds, native cases,
initial failures, raw captures, commands and runtime library mappings are retained.

The [base CI](https://github.com/Lulus792/NexaUI/actions/runs/42324010228) completed
all seven required jobs successfully. This step's native driver is a separate
explicit invocation, not an additional automatically passing CTest. Full root
suites from the [SDK navigation step](tree-navigation-report.md) remain attributed
to that source/run; only the shared portable fixture was rebuilt and executed
in the four consumer profiles here. New exact-source platform CI is separate.

Async child-loading/error UI, breadcrumbs, virtual realization and focus reveal,
richer native tree/table selection, actual component/picker services, installed
IMEs, production registry/session integration and human/physical acceptance remain
open. Files burst and held-pointer loss findings remain open. Aetheri can keep its
current foundations and select independently accepted SDK behavior; this evidence
does not update its pin or approve a 1.0.0 release.
