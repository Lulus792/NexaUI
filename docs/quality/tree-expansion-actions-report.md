# Native tree expansion actions — 7 November 2026

SDK/test source:
[`ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`](https://github.com/Lulus792/NexaUI/commit/ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36).
Base: `604a5aee2b3fd0869ae8a4d7275bde9e9e389f03`.
This fixes a concrete L06/I03/A02/A06/Q02 gap found while assessing async
hierarchy composition in delivery packet 6. Those full requirements remain open.

## Cause and resulting behavior

Tree rows/expander buttons and collapsible headers already advertised semantic
`EXPAND` or `COLLAPSE`. The Linux adapter exposed the corresponding named AT-SPI
Action. However, `nx_semantics_request_action` rejected both action kinds. A native
DoAction acknowledgement could therefore be followed by no application change.
The previous native tree checks covered keyboard expansion, not these actions.

The existing request API now accepts the currently offered action on a built-in
expansion control. A fixed frame-local request buffer retains its ID, semantic
role, parent and overlay. The next tree/header call validates that binding and
enabled ancestry before setting its requested expansion value. Missing,
disabled, reparented or incompatible replacement controls expire the request.
There is no allocation in queue admission; the existing 64-entry capacity and
duplicate rejection remain. Generic custom widgets without this state binding
remain unsupported, even if their semantic descriptor advertises expansion.

Expansion is a state request, not a toggle or click. It must not select the row
or steal focus; it is idempotent when the application already reached that value.
Queued native state requests apply after the frame's routed key/pointer input.
The application still owns its expansion, selection, data requests and operation
authority. Failed painting leaves its expansion unchanged; the same-frame retry
can still apply the request after successful semantic registration and painting.
Public struct sizes, symbols, ABI, dependencies and version 0.1.23 are unchanged.
The private context retains extra request metadata and regions record an expansion
capability flag; no performance or total-memory claim is made for this change.

## Portable tests and actual source profiles

`tests/test_expansion_actions.c` exercises row and expander actions, idempotent
state updates, duplicate/opposite/leaf/custom rejection, selection/focus retention,
disabled ancestry, omission, reparenting and replacement. It also checks the
existing collapsible-section binding. A controlled allocator forces an actual
command-reserve failure without assuming private allocation sizes, then verifies
unchanged app state and successful retry in the same frame. No assertions rely
on C `assert`, so Release/NDEBUG retains the checks.

| Installed software SDK profile | External portable CTests | Final regular root suite | Original Files CTests |
| --- | --- | --- | --- |
| Static Release | 2/2, no skips | 146 passed / 152 registered; 6 skipped | 4/4, no skips |
| Shared Release | 2/2, no skips | 145 passed / 151 registered; 6 skipped | 4/4, no skips |
| Static Debug ASan/UBSan/LSan | 2/2, no skips | 146 passed / 152 registered; 6 skipped | 4/4, no skips |

Shared omits the existing static-only selection-set target. The six root skips
are the ordinary native window, poll-fd, management, poll-many, poll-many reentry
and poll-many peer-destruction tests without a display. They remain skips. The
separate native workflow below covers selected ordinary-window/action behavior,
not all skipped native contracts. Root builds use their regular project flags;
the installed SDKs and external tree consumers additionally use global
`-Wall -Wextra -Wpedantic -Werror`. SDK/test builds are C17, with software/Wayland
and AT-SPI enabled, X11 and Vulkan disabled. ASan/UBSan use frame pointers, leak
detection and halt-on-error, without suppressions.

## Independent native Action and keyboard observer

The existing [native tree fixture](../../tests/tree_navigation_native/README.md)
still owns the same small hierarchy and ordinary public Wayland/software window.
Its new peer includes the existing independent OS helpers, no NexaUI headers and
no NexaUI/application linkage. It queries actions by their standard native names,
then verifies the actual row states and exact expansion event paths/values.
The private registry discovers the exact retained host PID.

Each of the three profiles passes eight native steps: collapse/expand a child
row, collapse/expand its expander, collapse a root row/expand its expander, then
collapse the root expander/expand its row. Exact root false/true and child
false/true event counts are each 2/2, with zero selection events. Focus remains
on separate row 17, selected ID remains 15 and the second tree keeps selection
31. Removed child references fail native reads; restored children have different
native paths. The driver requires a completed host view with the expected state
and zero key events; only the final Escape close uses keyboard input. Every
accepted host and peer exits 0 without forced kill.

The original 16-step native-key peer also passes on all three new SDK profiles,
including its focus, expansion, selection/deselection and retired-reference
checks. These are explicit private-seat invocations, not automatically passing
root CTests or human screenreader acceptance.

Two before/after boundaries detect the actual old behavior:

* The external portable test against the previous installed SDK fails at its first
  offered `EXPAND` admission. Both the initial test and final stronger test are
  built and executed against that old SDK.
* The same final shared native host executable, with only `LD_LIBRARY_PATH`
  selecting the old SDK, maps and accepts native Component focus and the offered
  collapse Action, but fails native step 0. Root/child remain expanded, focus is
  17 and selection is 15/31, with zero key events. The peer reports that exact
  state failure; the driver times out waiting for ACK 0. Mapped library source is
  `8dbd3d961e5e7d4a51a40919e15051e31a67a887`, SHA-256
  `5528b3e032fd316c63a25abe7689b5ae3fadc783049a6bdd764781cc991a2bc0`.

An acknowledgement or arbitrary setup failure is not counted as the expected
negative. All failures and cleanup results remain retained.

## Original Files and image regression

The unchanged original Files consumer is rebuilt against each new installed SDK.
Its original model/controller/two view tests all pass. Every profile also passes
the six original native peer modes and the 15 original-model navigation steps
from the [Files navigation report](files-navigation-report.md), including snapshot
limits, held ancestry, inode replacement, refresh, native retirement and Ctrl+Q
close. No document child launches. This rechecks shared assistive queue behavior;
it does not rerun document/Terminal actors, the complete original project runner
or Aetheri's normal exact 0.1.18 package/deployment policy.

All 96 complete PPM comparisons are byte-identical: 18 original-keyboard captures
against the preceding tree SDK, 72 original-Files captures against its preceding
navigation step, and six initial/final Release Action captures across the driver
fix below. Raw three-output P6 files are retained. Release Action output-0 images
were converted losslessly and inspected: collapsed child 15 is absent, restored
selection 15 and independent selection 31 are blue, while focus remains yellow
on row 17. This supplementary image evidence establishes neither compositor
presentation acknowledgement nor physical display timing or final product design.

The first native driver attempt completed all eight peer assertions, then wrongly
treated that verified peer's normal exit during the final capture as a child
failure. It now marks expected peer completion after the final ACK, while still
checking its exit status and verification marker. All three native positives use
that corrected driver. The initial failed case is retained. Portable paint-failure
retry was added during review; all three final external and root suites were
rebuilt as needed and rerun with the stronger test. Initial source extraction
produced future-mtime warnings in the old baseline build; newly copied source
mtimes were normalized before candidate builds. System clocks were not changed.

## Evidence and remaining adoption

The complete 1,027-file compiled source tree is matched to the code archive, using
explicit CRLF/LF normalization for text while retaining both physical hashes.
Installed libraries, builds, runtime maps, exact commands, initial/final tests,
before sources, native failure boundaries and captures are retained in
`D:/NexaUI/out/nexaui-expansion-actions-20261107.tar.gz`.
SHA-256: `2742559b8fa96e080deadb41777cbc3e765fd0f0df7dfde26e09c443b0d0c6cd`.
Size: 207,443,692 bytes. All 20,397 manifest files and 11 symbolic links were
independently verified after transfer to the local host.

Environment: Fedora 44 x86-64, Linux 6.19.10-300.fc44, GCC 16.2.1, CMake 4.3.0,
Python 3.14.3. The frozen Aetheri compositor and Liberation Sans bytes remain the
same as the preceding reports. Native OS peers are separate `-O2` C17 builds
against explicitly supplied private libatspi/GLib/GObject development inputs;
they are not claimed to be sanitizer-instrumented. Optional pkg-config CMake peer
targets were disabled for this private-prefix environment and compiled manually.

The [base CI](https://github.com/Lulus792/NexaUI/actions/runs/42329941425) on exact
`604a5aee2b3fd0869ae8a4d7275bde9e9e389f03` passes all seven required jobs;
two optional jobs were skipped. New exact-source platform CI remains separate.

Async hierarchy loading/error/retry composition, cancellation/late-result
correlation, focus restoration when the focused child disappears, generic virtual
realization/reveal, breadcrumbs, complete picker/component services and normal
Aetheri adoption remain open. Windows UIA ExpandCollapse and macOS native expansion
adoption are not added by these Linux checks. Performance, installed IMEs and
human/physical gates, the Files burst failure and held-pointer loss findings also
remain open. Aetheri's live source and pin are untouched; no release tag or 1.0.0
approval is inferred.
