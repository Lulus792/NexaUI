# Cancellable native close requests — W01 SDK foundation

Implementation `49d944e1817eca47c4b31efeb372673427edad2a`; final tested source
`47ec4e640a8a0f54bc8a09d1fe825b57956df6d7`, 8 November 2026. Development
SDK **0.1.27**, not a 1.0 release. The follow-up corrects the X11 test to consume
the second window's already delivered first close, preserving its original
isolation and clipboard-owner-loss assertions.

Follow-up: the [installed original Editor workflow](editor-close-report.md)
provides application-level controlled evidence on a separate consumer revision.

## Requirement and behavior

Catalogue W01 requires Cancel, another native close, continued editing/idle and
move/resize/zoom on the retained original window. The previous sticky
`should_close` flag could not acknowledge a request; subsequent management
was refused even after an application dismissed its own review. This step adds
the reusable SDK protocol. Full original Aetheri editor adoption remains open.

The owner thread calls `nx_window_take_close_request` after native polling,
before treating `should_close` as terminal. A nonzero request contains a borrowed
live SDK-window reference and a monotonically increasing per-window ID. Taking
it clears the legacy flag so ordinary waiting, input, rendering and management
can continue while the application reviews unsaved work. The token is delivered
once and remains pending until resolved or superseded. No allocation or
unbounded notification queue is introduced.

`nx_window_resolve_close` accepts only the latest actually taken token from that
window. CANCEL keeps the window usable; ACCEPT makes the legacy flag terminal
without destroying the window, renderer or model. Stale, foreign, superseded,
duplicate and untaken replies fail without changing the pending request.
Repeated native closes supersede older requests; unobserved notifications
coalesce to the latest. Forced native destruction, connection failure and
exhausted IDs cannot be cancelled or revive a window. IDs never wrap.

Existing descriptor/info/event layouts, legacy-only loops and resource-owner
order remain intact. Neither operation dispatches events. Tokens must not be
used after SDK-window destruction. See the [public contract](../concepts/window-management.md#cancellable-native-close-requests-0127),
[public declarations](../../include/nexaui/nexaui.h) and
[common bounded state machine](../../platform/common/close_request.h).

## Executed checks

| Environment | Actual result and boundary |
| --- | --- |
| Local Windows static/shared Release | Three affected CTests each: common close contract, core and native Windows management; six executed passes, no skips |
| Local Linux Wayland-only static/shared Release and ASan/UBSan/LSan | Fifteen executed CTests per profile; one default-display test explicitly skipped per profile, not counted as a pass |
| Local installed C17 package consumers | Five configure/build/run consumers across those profiles; exports, minimum version and existing window-info size/offset checked |
| Real older package | Installed SDK0.1.26 refused by `find_package(NexaUI 0.1.27 CONFIG REQUIRED)` |
| Native functional counterexamples | Two separately compiled Wayland backends remove stale-ID rejection or once-only delivery; the real protocol test rejects both at the intended assertion |

The initial Linux selection runs seven tests and one display skip. A separate
eight-case fractional-scale matrix per profile supplies the other eight executed
tests; the initial regular expression did not select their suffixed names.
All 79 actual C objects per Linux profile have C17 and `-Werror` compile commands.
The Windows records bind 47 actual objects per profile, including its installed
consumer, and the generated C17 projects. Windows ASan is not claimed locally.

Native Windows checks include actual WM_CLOSE, equal-ID tokens from two windows,
owner-thread guards, cancellation, valid subsequent move/resize/maximize/restore,
supersession and terminal WM_QUIT. Wayland checks use actual xdg close callbacks,
post-cancellation gesture requests and maximize/restore, while preserving the
existing seat/capability-loss and capture assertions. X11 checks actual
WM_DELETE_WINDOW delivery, isolation, cancellation, native resize and terminal
acceptance. AppKit checks performClose, foreign-window tokens, cancellation,
zoom/restore, repeated native pointer/Unicode/IME input and forced native close.
The common test additionally exercises sequence exhaustion without wrapping.

Exact-source [hosted CI run42509934413](https://github.com/Lulus792/NexaUI/actions/runs/42509934413)
passes all seven required jobs on `47ec4e6`:

| Job | Executed CTests, no skips |
| --- | ---: |
| Windows static / shared | 142 / 141 |
| macOS static / shared | 118 / 117 |
| Ubuntu static / shared | 259 / 257 |
| Linux sanitizers | 259, then 259 with reverse driver discovery |

Ubuntu static also passes one separate presentation-lifecycle repeat. All six
core jobs configure, build and run the installed close-request consumer.
Optional Windows-ASan and fuzz-campaign jobs are skipped. Hosted AppKit success
does not require the user's powered-off Mac and does not establish physical or
human acceptance. [CI receipt with native test lines and log hashes](close-request-ci.json).

## Source and artifact provenance

The local builds use completed staged bundle2, 1,028 files. Against `47ec4e6`,
951 files are byte-identical, 74 differ only in line endings, and three later
changes are explicitly separated: CI consumer steps, changelog and the corrected
X11 test. None of those three was compiled in local profiles; X11 was disabled
locally and its final source is covered by hosted CI. All 59 locally compiled
repository source paths match their bound Git inputs. The SDK builder-declared
revision is deliberately empty; this evidence binds actual bytes instead of
inventing an embedded revision. The contract document is committed separately
from the source bundle's excluded documentation directory.

The [source binding](close-request-source-binding.json),
[Linux objects/commands and 370 artifacts](close-request-local.json),
[Windows objects/projects and 138 artifacts](close-request-windows.json) and
[check summary](close-request-checks.json) preserve these boundaries.
Three [fully verified archives](close-request-archive.json) retain native
objects/binaries, installed prefixes, logs, mutants, source bundles and drivers.
Every recorded VM artifact and selected Windows artifact is bound to its archive
by physical size and SHA-256. Every archive member's bytes, mode and complete
directory/link set was checked, not merely its outer checksum. CI logs and
binding/build/archive drivers are retained in the third evidence archive.
Local archive paths are not published downloadable release assets.

An initial Windows configure ran before the first source export finished and
failed on missing inputs. Successful fresh builds use completed source2; the
initial source/project state remains archived. An attempted retirement of old,
already archived captures refused unreadable `/proc/954920/cwd` before deletion.
No capture, SDK prefix or original Aetheri tree was retired by this step.

## Remaining scope

W01 remains partial. Next, an independently installed consumer must compose the
existing editor/review with this protocol and prove Cancel, another compositor
close, continued editing/idle and movement/resizing/zoom on its retained window.
The original Aetheri editor must adopt and pass that workflow independently;
the installed ABI consumer here is not that application-level evidence.
No Aetheri source, pin, launch policy or default owner is changed.

The prior Software ASan AT-SPI Read-action instability remains unexplained.
All-output loss/resume, physical monitors/input, human assistive/design review,
performance budgets, full service lifetimes/adoption and the remaining packet6,
packets7–8 and all103 catalogue IDs retain their full scope. This checkpoint
does not close the complete W01 row or declare the overall goal complete.
