# Asynchronous hierarchy composition — 7 November 2026

Consumer/test source:
[`345394940ab4bb2d8b820efb9364cdfeca49baad`](https://github.com/Lulus792/NexaUI/commit/345394940ab4bb2d8b820efb9364cdfeca49baad).
Base: `10688ef9a9f4519f02176ecf340bde19e1bc1ed2`.
Reused installed SDK source: `ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`.
This advances scoped L06/L11/I01/A02/A06/A10/D02/P04/P05/Q02/Q04 evidence in
delivery packet 6; it does not complete those whole requirements.

## Existing APIs and explicit ownership

The [new installed-SDK consumer](../../examples/30_async_hierarchy/README.md)
composes existing tree, status, button, scroll, font/theme, command/completion,
Wayland/software and poll-FD APIs. It adds no SDK API, ABI, runtime dependency or
version change. The neutral producer returns declared Child A/Child B records;
this is neither another Aetheri model nor an implementation of its Files scans,
component catalogue, picker, grant protocol or privileged service.

The view owns expansion, selected UI ID, copied response rows, cached layouts and
the current publication target. Workers own copied request keys and completion
payloads. Neither a worker nor a message contains a view/context/window pointer.
Accepted commands reserve completion capacity through `nx_sim_channel`; every
consumer disposes its taken message even when it ignores the result. The existing
channel and font collection outlive each view and its layouts.

The owner accepts a Load/Retry/Refresh intent only after a successful context
frame. The UI distinguishes unqueried, loading, failed, ready and empty states;
an action acknowledgement is not a successful observation. Loading exposes Busy
status. Failure exposes Retry without implicit resubmission. Repeated Right while
loading submits no duplicate work; cached collapse/re-expansion submits no fetch.
Queue-full/allocation errors are retryable failures rather than false completion.
Generation exhaustion cannot wrap and disables Retry with a reopen-view message.

Only a matching active ticket, view epoch and request generation can publish to
an expanded loading branch. Collapse invalidates publication without pretending
to cancel already taken backend work or release its channel reservation. An
unrelated/retired result is drained and ignored. A malformed active reply fails
explicitly rather than leaving an unfinishable busy state. Refresh supersedes
older work; view replacement retires the old native context before allocating
its successor. Real data/session changes must also invalidate requests at their
actual owner; this composition cannot discover or reconcile domain changes.

This example bounds a response to 16 unique child IDs and 96 bytes per name,
validates names/IDs before copying and uses a scroll area for its bounded rows.
That is an example contract, not a new limit on NexaUI trees. Focused rows and
Retry/Refresh are revealed; removal restores branch focus. When needed, a second
frame is built before publication so native focus and the drawn cue agree.
Selection survives cached collapse and is reconciled on an accepted observation
that removes the selected child. Large/paginated owner hierarchies still need
their virtualized composition and actual data/service integration.

## Executed portable and native cases

| Installed SDK profile | Portable CTest | Native workflow | Producer/window end |
| --- | --- | --- | --- |
| Static Release | 1/1, no skips | 18/18 stages | Both workers joined; host/peer exit 0 |
| Shared Release | 1/1, no skips | 18/18 stages | Both workers joined; host/peer exit 0 |
| Static Debug ASan/UBSan/LSan | 1/1, no skips | 18/18 stages | Both workers joined; host/peer exit 0 |
| Shared, German, 200% text, 360×260, high contrast | Same portable binary as above | 18/18 stages | Both workers joined; host/peer exit 0 |
| Static Release, light theme | Same portable binary as above | 18/18 stages | Both workers joined; host/peer exit 0 |

The portable test has four groups: state/intent/selection transitions; stale,
out-of-order, capacity and view-replacement cases; malformed replies and generation
exhaustion; and bounded last-row reveal/frame-construction failure. It uses real
channel messages with manually controlled worker interleavings. Checks remain
active under NDEBUG. A failed small frame posts no command; a focused last child
among 16 is fully inside its scroll viewport; refreshing removes that child and
restores branch focus. Matched wrong-key, oversized, duplicate-ID and unterminated
name payloads become failures. No generic parser/fuzz or arbitrary-allocation
failure coverage is inferred.

The native host uses two actual pthread workers held by explicit condition-variable
permits. They post results to the existing bounded channel and wake the owner
through an application FD. The UI calls `nx_window_poll_with_fd` with
`UINT32_MAX`; no frame timer or polling worker is added. A 200-ms held-job interval
observes no unsolicited frame or duplicate request. That is a scoped observation,
not a CPU/energy, latency or idle-performance budget.

An independent OS libatspi peer discovers the exact retained host PID and checks
actual status/Busy, children, selected/focused states, full native viewport
containment, retired references and fresh identities. Absence requires a complete
bounded native scan without read errors. Native Component/Action and actual
Wayland keys drive Retry, selection, cached collapse, explicit refresh and view
replacement. Results are deliberately completed out of order and after replacing
the view; stale results must not change current phase, revision or ticket.

Each final workflow submits/completes seven requests: four observations adopted,
two stale replies ignored, and one result drained after UI teardown. The host
destroys native semantics, view, renderer, context and window before releasing
the last held worker. That worker completes to the still-live channel/pipe, both
workers are joined, remaining payloads are disposed and channel destruction
succeeds. All five accepted cases exit 0 without forced kill or sanitizer
diagnostics. This proves the fixture's owned lifetime, not arbitrary reentrant
callbacks, production service cancellation or general thread safety.

## Counterexamples and source/image verification

Two isolated copies of the presentation module validate the observer. Removing
ticket/phase/key publication guards fails its portable stale-result assertion
and native stage 11: an old successful reply wrongly replaces the newer empty
view with two children. Ignoring the observed error status fails the portable
failed-state assertion and native stage 2: an actual failure wrongly appears
empty and lacks Retry. Both reach their intended checks with the same installed
shared SDK; an arbitrary setup failure is not counted as a negative. Sources,
exact changes, builds, logs and failure cleanup are retained separately.

External consumer builds use C17 with
`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`. The installed SDK
artifacts retain their preceding strict C17 profiles and are reused byte-for-byte.
ASan/UBSan use frame pointers, leak detection and halt-on-error, without
suppressions. The separate OS peer is an `-O2` C17 build with global Werror and
no NexaUI linkage; it is not claimed to be sanitizer-instrumented. Its optional
pkg-config CMake target is not exercised in the guest's private-prefix setup;
the exact manual OS-development-prefix compile/link command is retained.

The first shared consumer link exposed its own unnecessary `fmaxf`/libm use;
a finite row-height comparison removes that call. The failed source/log remain
retained. Native absence checks were strengthened before final acceptance.
Exhaustion feedback and cache/reveal checks were completed before final builds.
A final header-comment correction produced identical host/core executable hashes
after rebuilding all three profiles, preserving the actual tested binaries.

All 1,036 consumer source files match the code revision with explicit text
CRLF/LF normalization and both physical hashes. All 159 selected SDK production/
build/data inputs match, except the root CMake file's exactly verified four-line
new test registration; removing only that block yields the SDK CMake input.
No new full regular-root suite is claimed locally: the new portable target is
registered for normal CI, while the three standalone consumer suites run here.

Final dark, light and German/high-contrast/200% cases retain 45 complete raw PPMs
across three synthetic outputs. All 45 are byte-identical to their matching
preceding runs. The final German small-window error/Retry and light ready/selected
images were converted losslessly and inspected: the error is fully readable,
Retry has a visible cue, and selected/focused child B remains visible. These are
supplementary virtual-output checks, not physical presentation acknowledgements
or final Aetheri product-design acceptance.

## Evidence and remaining adoption

Archive: `D:/NexaUI/out/nexaui-async-hierarchy-20261107.tar.gz`.
SHA-256: `c0b1bfabe0579face54bdfe0a8146f7c79abc54d128bc0f3179572d8745dd20d`.
Size: 28,482,439 bytes. All 1,663 manifest files and two symbolic links were
independently verified after transfer to the local host. Raw images, earlier
attempts, sources, binaries, SDKs, process maps and all commands are retained.
After that verification, only 105 matching loose PPM duplicates from these new
VM cases were retired, recovering 1,475,713,785 bytes. Every duplicate was checked
again against the retained archive manifest before removal; the archive remains
on both machines. Earlier frozen evidence, sources, binaries and logs were untouched.
The supplemental retirement record is
`D:/NexaUI/out/async-hierarchy-capture-retirement-20261107.json`.

Environment: Fedora 44 x86-64, Linux 6.19.10-300.fc44, GCC 16.2.1, CMake 4.3.0,
Python 3.14.3. The frozen Aetheri compositor and Liberation Sans bytes remain
unchanged from preceding reports. Guest/host clocks differ; only newly copied
source mtimes were normalized. No installation or global clock/driver change
was made.

The [base CI](https://github.com/Lulus792/NexaUI/actions/runs/42331805332) on exact
`10688ef9a9f4519f02176ecf340bde19e1bc1ed2` passes all seven required jobs, with two
optional skips. New exact-source platform CI is separate. macOS/Windows native
hosts, TSAN, real hierarchy/data-owner services, broader multiple-branch/paged
composition, full pickers, grants, permission/conflict/reconnect, durable recovery,
performance budgets and physical/human acceptance remain open. Original Files
scans remain synchronous. The sample view requires bounds of at least 280×160;
arbitrary resizing and fallback below that constructor bound need host handling.
The separate Files burst and held-pointer findings
remain open. Aetheri's live source/pin and version-release approval are unchanged.
