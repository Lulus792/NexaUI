# Directional tree navigation — 7 November 2026

Code and regression:
[`8dbd3d961e5e7d4a51a40919e15051e31a67a887`](https://github.com/Lulus792/NexaUI/commit/8dbd3d961e5e7d4a51a40919e15051e31a67a887).
Base: `592742f0a2696ff0f7b0ea18bfdcbefe7b79b302`.
This advances L06/I01/Q02 within the existing 103-ID catalogue.

## Behavior and ownership

`nx_tree_item` now registers private row/parent/group metadata with its existing
interaction regions. Up/Down and Home/End move logical focus among eligible
registered rows in the nearest tree or table, stopping at the ends and skipping
expander-button stops. Right opens a closed branch or enters an already published
child; Left closes an expanded branch or returns to its parent item. Parent/child
relationships follow semantic item ancestry. Separate table cells support row
navigation without inventing a logical hierarchy from their coordinates.

Navigation excludes disabled/inactive rows and subtrees under collapsed or
disabled/inactive item ancestors. A collapse takes effect for subsequent keys
in the same batch. Per-event expansion stays at its original row/expander even
when that key also moves focus. Independent trees and active modal scopes retain
their own eligible targets. Consumed keys do not navigate; key repeat can move
focus without becoming selection or activation.

Application-owned expansion and selection remain distinct. Navigation alone
does not select an item, fetch children or issue a file/device/software action.
The application registers newly loaded children, realizes offscreen virtual rows,
reveals focus and chooses restoration after removal. Targets come from the last
completed frame; unavailable children are never fabricated. Tab and explicit
Enter/Space selection/expander activation remain available. See the
[hierarchy contract](../concepts/inspector.md).

The change adds no public symbol, descriptor field, dependency or version change.
Private region storage grows by two IDs and state flags, reused with the normal
region buffers. The navigation helper itself allocates no memory. This is not a
total-memory, idle-power or performance-budget claim. Existing widget rollback,
semantic/action registration and application models are reused.

## Regressions and counterexamples

`nexaui_tree_navigation` is a new ordinary 15-second CTest, linked to the public
SDK on every platform. It checks 19 sequential hierarchy steps, five second-tree
steps, disabled and unloaded-child transitions, five multi-event batches and
table/modal scenarios. Assertions read final logical focus and semantic focus,
application expansion and unchanged selection, plus explicit Enter selection.
Cases include no wrapping, row versus expander focus, repeat, consumed input,
collapse followed by Down, two-level movement and modal focus restoration.
The previous nine event-target regressions remain unchanged and pass separately.

External static/shared Release consumers linked to the previous installed
`bae657a4cdaaed6e4f6dc426b71ad17c69586c12` SDKs reject the new regression at its
first step: focus remains 11 instead of moving to 13. Their old tree tests still
pass, separating new navigation from the preceding event-target fix. SDK
production sources are unchanged between that revision and this step's base.

The first uncommitted implementation passed its initial scoped tests but a
review probe disabled only the parent item, leaving its child flag clear. Down
then entered that subtree. The exact failed probe, its linked initial SDK and
initial source are retained. The final ancestor eligibility check fixes this;
the stronger parent-only case and additional table/modal tests are in the final
regression. Initial and final source/build trees are separate; previous evidence
is preserved. A passing first probe set is not treated as final acceptance.

## Build and acceptance boundaries

Final SDK and external C17 consumer builds use
`-Wall -Wextra -Wpedantic -Werror`. Static/shared Release and static Debug
ASan/UBSan/LSan profiles are checked. SDK builds emit no warning/error diagnostics;
the final consumers execute both old-tree and navigation regressions. Root suites
use normal project warning flags in separate Wayland-only, software, AT-SPI-enabled
builds, with Vulkan and examples disabled. Sanitizers retain leak detection and
halt-on-error, with no suppressions.

| Final root profile | Passed | Registered | Skipped |
| --- | ---: | ---: | ---: |
| Static Release | 145 | 151 | 6 |
| Shared Release | 144 | 150 | 6 |
| Static Debug ASan/UBSan/LSan | 145 | 151 | 6 |

The extra registered case is the new navigation regression. Shared omits the
existing static-only selection-set case. Each profile retains six external-display
skips: window, poll-fd, native management, group polling, reentry and peer
destruction. These remain skips. Existing assertions, warning settings and timeouts
are preserved; no separate native rerun is substituted for these results.
Environment: Fedora 44 x86-64, Linux 6.19.10-300.fc44, GCC 16.2.1, CMake 4.3.0
and Python 3.14.3. Release is `-O3 -DNDEBUG`; sanitizer Debug also uses
`-fsanitize=address,undefined -fno-omit-frame-pointer`.

All 1,015 final source files match the code archive, with explicit CRLF/LF
normalization for text. The new test is exported with CRLF by Git archive and
compiled with LF; its complete normalized contents match and both physical
hashes are retained. Initial and final sources, installed SDKs, final root builds,
consumer binaries, logs/JUnit, negatives and helper sources are retained with a
file manifest. Guest/host clock differences remain explicit; only copied source
mtimes were normalized, without changing system clocks.

Evidence: `D:/NexaUI/out/nexaui-tree-navigation-20261107.tar.gz`, SHA-256
`c4990588a049623a74cf547a42f03d98d445cb4696988394fbd096c6681cd61e`.
All 8,550 manifest files and ten symbolic links were independently verified
after transfer to the local host.

These are public-event core/protocol checks, not a new native compositor,
independent assistive-client, physical-keyboard or human visual campaign. Native
focus/selection notifications, offscreen reveal/materialization, async child
loading/error UI, breadcrumbs, richer tree-table relationships and real component/
picker/service adoption remain open. The Files burst and held-pointer loss
findings remain open. Aetheri's sources/pin, development SDK 0.1.23 and the 1.0.0
approval boundary are unchanged.
