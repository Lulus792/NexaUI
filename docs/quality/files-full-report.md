# Files original document workflows on SDK0.1.33

Consumer source: `e7138808ede25a08bcc568a093a2d7de425e56d4`.
Installed SDK: **0.1.33**, source
`1e00a6f4f3a0ab3a7da1aa35a36f02679421366b`.
Original Files owner: `7483286bcaafbeaced5831f72cb4d0e0cad9a158`;
private producer: `d56d69523c33a48648af343e7d36526f84518c90` with the existing
two recorded generated producer files. Aetheri sources, pins, builds and live
desktop remain independent.

The original document workflows now pass on three fresh glibc builds using the
actual Files launch implementation and real original Terminal/editor children.
This closes the unavailable-libc launch gap of the previous scoped musl check.
It does **not** establish complete visual/frame parity: four sanitizer captures
show intermediate input/focus states at this checkpoint. The later
[read-only observation follow-up](files-observation-report.md) closes those four
known gaps with unchanged app/SDK binaries; the historical results below remain intact.

## Actual bug and bounded change

The original native driver does not enable the compositor's optional text-input
protocol. The generated Files host previously required
`nx_window_set_ime_transaction_events(window,1)==NX_OK` and exited with
`CLOSED files frames=0 failed=1` when that call returned
`NX_ERROR_UNSUPPORTED`. The earlier IME-only driver explicitly sends `I on`,
which concealed this startup failure.

The host now accepts **only NX_OK or NX_ERROR_UNSUPPORTED** at that boundary.
It still binds the window clipboard, retains ordinary keyboard editing and
rejects other transaction-initialization errors. The existing SDK text-field
and end-frame paths already tolerate an unavailable native IME. No SDK API,
production source, version, font discovery or dependency is added.

`run_native.py` additionally accepts an explicitly checked0.1.33 SDK and a
`--linkage static` declaration. Static runs verify the exact archive and app
hash, require no mapped NexaUI shared library and retain all original behavior,
assertions and deadlines. Shared selection remains the default. The separately
linked fault host remains test-only; ordinary binaries contain no fault hooks.

The failed pre-fix native run and its frozen source are retained. The same
original no-IME workflow passes after the change; enabling IME in that workflow
was not used as a workaround.

## Executed checks

Environment: existing ordinary UID1000 Fedora44 VM, glibc2.43, GCC16.2.1,
private verified CMake/Ninja and pinned native dependencies. No VM restart,
global installation, mount/quota/driver change or physical/human task resumption.
Builds keep C17, strict warnings as errors and the original limits.

| Profile | Original/component CTests | Original normal document/AT-SPI | Original lost-result worker | Separate native IME/clipboard |
| --- | ---: | --- | --- | --- |
| Shared Release | 27/27 | passed | passed | 350 checkpoints |
| Static Release | 27/27 | passed | passed | 350 checkpoints |
| Static Debug-O1 ASan/UBSan/LSan | 27/27 | passed | passed | 350 checkpoints |

All **81 CTests and nine native tasks** execute, without test skips or a spawn
tripwire. Sanitizer halt-on-error and leak detection remain enabled. The full
component suite includes original Files launch/create/rename/move, editor
document/modal/history/search/selection/file/view, Terminal model/selection/
paste/composition, view scales, IME pressure, font admission and naming layout.

Each normal task opens the actual original Terminal and modal editor, verifies
their executable hashes, direct parents and pidfds, accepts real native input,
saves exactly `hello`, and observes document end. It also covers held-directory
navigation, create/rename/move, native chrome/resize/cancel, independent libatspi
selection/text/query/actions, a disabled-provider negative, original short idle
CPU/redraw guards and clean close. Those guards are not performance budgets.
The Terminal child still uses its original SDL presentation; it is not counted
as a new NexaUI Terminal implementation.

The nine fault cases across the three profiles perform actual mkdir/rename/move,
hold the original worker after the effect, require one syscall attempt, preserve
bytes/inodes where applicable, block replay and wait on close. Separate IME
tasks retain the350 pressure/clipboard/create/rename checkpoints per profile,
including deliberate real NoReply rejection and five font-startup refusals.

Five additional pre-launch counterexamples reject a wrong SDK revision/version,
changed shared-library hash, changed static-archive hash and changed app hash.
All fail with their expected diagnostic before creating an output or native app.
Three installed C17 probes report0.1.33/source1e00a6f and features116. The source
audit binds321 SDK/consumer translation units and104 original Files/producer
inputs; actual ELF dependencies and ordinary/fault separation are checked.
No remaining process refers to this check's owned root after completion.

## Capture evidence and remaining observation gap

The nine tasks retain **414 lossless captures** across three virtual outputs.
All276 full shared/static/sanitizer comparison pairs are checked byte-for-byte;
**272 match**. All138 shared/static-Release pairs and all IME/fault pairs match.
The four differences are in the normal sanitizer task:

| Capture | Observed difference |
| --- | --- |
| Body press/caption exclusion | Focus/hover outlines differ. |
| Inline grid folder | Intermediate `wok` versus `work`; the later actual directory worker verifies `work`. |
| Mouse query selection | Completed selection highlight is absent in the sanitizer capture. |
| Minimum German naming | Intermediate `d` versus `draft`. |

The original capture helper waits a fixed80ms, without an application input/
paint fence at these points. The original native assertions still pass; this
does not prove that each capture observes the completed interaction. Do not
mask pixels, increase that delay or claim a resolved application/observer cause.
The later [observation follow-up](files-observation-report.md) adds independent
read-only native state expectations while retaining the original tests and deadlines.

Eight complete selected captures and both images of all four differing pairs
were visually inspected. Initial list, invalid-name caret, complete German
Abbrechen/OK captions, unknown-result inspections and IME rejection states are
visible within the app bounds. The primary-only normal task still shows a
missing-glyph box for an unsupported symbol. The minimum German search hint is
shortened and the preceding `Moved` status remains English. These are not full
font/localization/design acceptance. [Images and comparisons](files-full-images.json).

## Evidence, historical attribution and remaining scope

[Local commands/results](files-full-local.json),
[source/artifact binding](files-full-source-binding.json),
[pre-fix binary and raw-source/protocol checks](files-full-preimage.json),
[retained raw archive](files-full-archive.json).
The raw archive is58,484,219 bytes, SHA-256
`4cc56323fb89253aed226cb7d83f1b1de5d55e365ee87d5e9e1c800a097c7f16`;
all1542 regular members and two symlink declarations are verified after transfer.
Borrowed fonts and external dependency/runtime libraries are not bundled; their
exact input checks/manifests and original archive references remain recorded.
The separately verified completion bundle adds the pre-fix executable, generated
SDK source bytes, controller/audit receipts and all seven CI logs; its93 regular
members and exact hash are listed in the archive receipt. The primary archive
remains unchanged.

[CI42753051344](https://github.com/Lulus792/NexaUI/actions/runs/42753051344)
passes all seven required SDK/native/package jobs on the exact consumer source,
first attempt. Full decoded logs are retained and every reported passed test
line is checked: Windows148/147, macOS124/123, Linux272/270, and sanitizer272/272
in the two fixed driver orders. The static Linux job also completes its separate
one-test presentation repetition. Two optional jobs are skipped; no CTest skips
are counted. These SDK/package checks are separate from the81 original Files
consumer CTests and nine private glibc native tasks. [CI receipt](files-full-ci.json).

Earlier setup failures remain failures: the first glibc controller passed81
tests but failed its producer link; an initial package probe selected the old
0.1.23 library and was rejected; a missing original libXi SONAME alias prevented
the first independent reader link. Fresh corrected fixtures use independently
checked library bodies and derived ELF SONAME aliases. The no-IME startup
failure was then exposed and fixed. These are not relabelled successful runs.

The previous pressure documentation incorrectly described the0.1.26 result as
new native document-child acceptance. Correct attribution is0.1.26 full component
and native-IME coverage, with original0.1.23 native document evidence separately
recorded. This correction leaves old immutable raw archives intact. The present
glibc result independently exercises the original document workflow on0.1.33.

Affected evidence includes B01/B03/B04, T04, I01/I05, A02/A04, P04/P05 and
Q02/Q03/Q06/Q08. All103 catalogue statuses remain unchanged. The four capture
observations have a later scoped follow-up; wider interruption/provider/locale/IME coverage, actual current
Aetheri compositor and owner adoption, trusted/device/software compositions,
packets7–8 and physical/human/design/performance acceptance remain open.
Aetheri can independently review/adopt this bounded Files host correction;
there is no automatic pin/default update or1.0.0 release.
