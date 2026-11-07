# Original Files navigation and held folders — 7 November 2026

Consumer/test source:
[`e12dc7f41520bd17ac4a0dac2f87f203d220a3fd`](https://github.com/Lulus792/NexaUI/commit/e12dc7f41520bd17ac4a0dac2f87f203d220a3fd).
Base: `fa7330b4df4dcc466daadeb269957bf62c211b22`.
Installed SDK source: `8dbd3d961e5e7d4a51a40919e15051e31a67a887`.
Original Aetheri application: `27de5cb228da0f37264fad64d964ad7a660d210d`.
This advances L05/L06/I01/A06/D07/P07/Q04 within delivery packet 6; those whole
requirements remain incomplete.

## Original application and independent native checks

The existing [standalone original Files consumer](../../examples/29_aetheri_owned_files/README.md)
now imports the original model test as well as the session and two view tests.
Its original `calloc` failure injection, complete-snapshot limit, held-directory
and inode checks remain unchanged. All 39 selected original inputs are hashed
and checked at configure and native-driver startup. The optional document group
remains a separate 20 inputs. No competing Files model or application view is
introduced, and the original exact SDK 0.1.18 package/deployment checks are neither
edited nor claimed to pass by selecting this external installed 0.1.23 SDK.

The new native peer compiles the pinned original peer helpers and original entry
point under a different name, then runs its own additional workflow. It includes
no NexaUI headers and links no NexaUI or Files code. It finds the exact ordinary
host PID through the private OS AT-SPI registry. The driver first runs all six
unmodified original modes, then synchronizes 15 additional steps with the new
peer. Original helper assertions and time bounds remain unchanged.

Native Component focus and Selection select the actual original rows; native
Action invokes original sidebar/Up controls. Enter, End, Backspace, Ctrl+R and
Ctrl+Q travel through the unchanged compositor's evdev control channel and the
real Wayland/XKB adapter. Key steps require a later diagnostic app frame before
the peer checks native state. Headings/status must be matching direct labels of
the semantic root panel, avoiding duplicate sidebar labels. Checks for absent
entries require successful bounded enumeration, not a failed lookup.

The driver modifies only its fresh private fixture: it removes one literal empty
file, renames two directories and creates replacements with known bytes. It
checks ordinary transient device/inode pairs and the host's held directory FD
targets. These are process-local filesystem observations, not durable storage
identity, grants, caller admission or a privileged service protocol.

| Steps | Required behavior |
| --- | --- |
| 0–2 | Enter two nested directories; a directory with 4,097 entries is rejected, retaining count, selected directory, depth and model revision. Its original limit error is exposed natively. |
| 3–5 | After removing the literal 4,097th fixture file, admit 4,096 entries. End selects/focuses the last row and fully reveals it inside the native viewport. Its earlier first-row reference becomes defunct and rejects action; Backspace returns to the original parent. |
| 6–8 | Rename an already held ancestor and replace its old name with a decoy directory. Its original sidebar button must still visit the held original directory, not the decoy. Returning to the held root admits a fresh snapshot from which the replacement is explicitly selectable. |
| 9–12 | After selecting another directory snapshot, rename and replace that directory. Enter must reject the changed inode and retain the root revision. Ctrl+R admits a fresh snapshot, retires the old row reference and creates a different native path. Only that fresh selection may enter the replacement. |
| 13–14 | Up and Backspace at the selected root retain the root and report its original boundary error. The original Up button is enabled; a successful native Action means delivery, not successful filesystem navigation. |

The last visible row was `item-4095`, with native window-coordinate bounds
`214,587,532,44` inside Entries `214,164,532,488`; ten rows were materialized in
the observed final Release frame. The peer requires full containment, selected
and focused states, the matching native Selection result and at most 128 children.
This is the original app's bounded reveal behavior, not generic offscreen tree
realization or a 100,000-file application.

## Executed profiles and counterexamples

| Installed SDK / consumer profile | Original CTests | Original native peer modes | Additional navigation | Close |
| --- | ---: | ---: | ---: | --- |
| Static Release | 4/4, no skips | 6/6 | 15/15 | Ctrl+Q, exit 0 |
| Shared Release | 4/4, no skips | 6/6 | 15/15 | Ctrl+Q, exit 0 |
| Static Debug ASan/UBSan/LSan | 4/4, no skips | 6/6 | 15/15 | Ctrl+Q, exit 0 |

All final positive runs retain the expected original/replacement fixture bytes
and 4,096 final full-directory files, launch no document child and report
`CLOSED files ... failed=0`. Sanitizer halt-on-error and leak detection are enabled,
without suppressions. No accepted case requires SIGKILL. These are four selected
original CTests per profile and an explicitly invoked native workflow, not a new
full-root or full-original-application-suite run. The six original peer modes
also cover the pre-existing default driver path before the additional workflow.

Two separately built test-only application variants validate the new observer:

* Reopening depth-1 ancestry by its old name instead of using the held descriptor
  fails at step 6. The host actually enters the one-entry decoy at depth 1, and
  the native peer rejects the missing original nested directory.
* Omitting the entry's inode/type check after `openat` fails at step 10. The host
  incorrectly admits the replacement at depth 1/count 1, rather than retaining
  the depth-0 snapshot and reporting the changed-entry error.

Both are built against the same installed shared SDK and run with the final
peer. They reach the intended native checks before failing; arbitrary setup
errors are not counted as negatives. Their isolated model preimages, exact
changes, binaries, commands and failure logs are retained. The immutable original
source, its configure hash guard and production SDK are unchanged.

## Source, image and build evidence

The three installed SDK artifacts from `8dbd3d9` are reused byte-for-byte. All 159
selected SDK production/build/data inputs match this consumer tree. Its complete
1,024-file source tree matches the code revision, with explicit CRLF/LF
normalization for copied text. All 39 original selected inputs and the complete
original archive SHA-256 are verified. Original model test Git blob:
`5e076ef3f0fab519aba8bcdbabe0c3680c238fa5`.

The model/controller/view/native builds use strict C17 with global
`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`. The independent peer is
a separate GCC `-O2` C17 build with `-Wall -Wextra -Wpedantic -Werror` and explicit
private OS libatspi/GLib/GObject headers/libraries. Guest pkg-config does not
discover that private development prefix, so the peer is compiled separately;
the optional CMake peer targets are not claimed as built by those configurations.
The peer itself is not claimed to be sanitizer-instrumented.

Environment: Fedora 44 x86-64, Linux 6.19.10-300.fc44, GCC 16.2.1, CMake 4.3.0,
Python 3.14.3. The unchanged frozen Aetheri compositor build from
`d56d69523c33a48648af343e7d36526f84518c90` has SHA-256
`631ec1740d9de6daac790d2c29351df7ccb2cabce936ac6ab35ba59b8cb71c75`.
Liberation Sans SHA-256:
`76d04c18ea243f426b7de1f3ad208e927008f961dc5945e5aad352d0dfde8ee8`.
Host and guest clocks differ; only newly copied source mtimes were normalized.

Each positive retains eight complete captures of all three synthetic outputs.
All 72 final PPMs are byte-identical to the preceding positive run's counterparts
before absence-check strengthening. Release output-0 limit/error images were
converted losslessly and inspected, as was the preceding run's identical last-row
image. The limit retains the two-entry parent; the last row is fully visible with
selection/focus; changed-inode rejection retains the three-entry stale root
snapshot, including its old selected `renamed` row.
These captures supplement native assertions; they do not establish physical
presentation timing or human visual/screenreader acceptance.

Initial fixture failures are retained: the peer first assumed FRAME/WINDOW for
the original root panel and then incorrectly required Up to be disabled at root.
Both assumptions were corrected against actual original source/native behavior.
Review subsequently strengthened absence checks and all three positive and both
negative workflows were rerun. An initial evidence collector used the wrong JSON
archive-key name; its terminal error is retained before the corrected collection.

## Remaining adoption

Evidence archive: `D:/NexaUI/out/nexaui-files-navigation-20261107.tar.gz`.
SHA-256: `2036d9845eb14d42bc233572a8700d5a9b4da77925b74771bf2f22da533a5f63`.
Size: 39,558,871 bytes. All 51,234 manifest files and five symbolic links were
independently verified after transfer to the local host. The archive retains
exact sources, installed SDKs, original input archive, consumer/counterexample
builds, all attempted native cases, raw captures, process maps and commands.

The [base CI](https://github.com/Lulus792/NexaUI/actions/runs/42326708802) on exact
`fa7330b4df4dcc466daadeb269957bf62c211b22` completed all seven required jobs
successfully; the two optional jobs were skipped. New exact-source CI is separate.

Original directory scanning is synchronous. Async child loading/cancellation,
loading and retry compositions, a full picker, sort/grid workflows, component/
device/software services, durable recovery and complete owner adoption remain
open. Original status labels expose names here, not newly adopted standard Text
interfaces. Performance budgets, installed IMEs and human/physical acceptance
remain open, as do the separate unpaced Files sanitizer queue-exhaustion and
held-pointer-loss findings. No public API/ABI, dependency, version or Aetheri pin
changes; no 1.0.0 approval is inferred.
