# Keyboard reading of complete owner results — 2026-11-07

Consumer source: `68908d0822d39ce3748160be20c670c45532bc42`.
Installed NexaUI remains `ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`, version
0.1.23. This follows the [persistent-owner composition](file-picker-owner-report.md)
and changes no SDK API/ABI, dependency, live Aetheri source or pin.

The owner's scrollable body previously had no application PageUp/PageDown path.
In a small window, the selected name could remain below the fold while keyboard
focus stayed on the owner controls. `fp_owner_key` now moves that body by half
of its actual viewport height, preserving focus. It does not intercept the note
editor, modified/non-key-down events, compact layouts without the scroll area,
or an active chooser's commands. The native host calls it through the existing
ordered input dispatcher. Existing scroll bounds clamp both ends; no new timer,
layout cache or global shortcut is introduced.

## Executed evidence

The same Fedora 44 VM, GCC 16.2.1, CMake 4.3, Python 3.14.3, real Liberation Sans,
original Files `27de5cb`, frozen Aetheri `d56d695` compositor and private AT-SPI
runtime are used. Static Release, shared Release and static ASan/UBSan/LSan each
pass **3/3 CTests**, without skips or suppressed diagnostics. All strict warning
flags remain enabled. The new real-font owner assertions read a 1,020-byte
escaped display label down to its final geometry and back to the top, retain
footer focus and original semantic bytes, and reject paging interception in
the note/editor, modal and modified/key-up cases. Existing worker, view and
owner lifetime assertions remain in those same suites.

Five native workflows select an **actual 255-byte non-UTF-8 filename**, whose
original model supplies the full 1,020-byte escaped label. The independent
AT-SPI peer has no NexaUI linkage and finds the original full row/result name.
It measures the result and owner viewport, focuses fixed Close, and checks
every native PageDown/PageUp frame for monotonic overlapping movement, stable
focus, reachable final bounds and return to the original top. The driver waits
for an actual frame and a distinct peer acknowledgment after every key.

| Native case | Reading in each direction | Additional coverage |
| --- | ---: | --- |
| Release dark, small | 5 pages | Ordinary note/file/folder/cancel/result lifetime |
| Shared German 200%, small/high contrast, 4,096 entries | 16 pages | Exact mapped shared library, captures of final text and fixed Close |
| Sanitizer equivalent | 16 pages | Active leak detection; no sanitizer diagnostic |
| Release light, small | 5 pages | Controlled preedit/commit/focus-loss cancellation and preserved committed note |
| Sanitizer equivalent, held Refresh | 16 pages | Cancel/owner usability and GUI retirement before worker release/Join |

All five host/peer exits are 0, with no forced cleanup. The complete original
name survives reading. Subsequent folder replacement closes the old file witness;
Cancel preserves the two-result count, note, remembered folder and trigger focus.
The existing device/inode, stale-reference and held-worker checks still run.
The prior step's fifteen other native regressions are historical evidence on
`951e8d2`, not reruns of this source; the actual five focused native executions
above and all current CTest suites are the claims of this step.

A disabled-paging core mutant fails at the new reading assertion (exit 1).
The exact previously published native host from `951e8d2`, SHA-256
`81b8e528204cebaa5c909212267e2353f6176c313cfde5e17a6ca9e6b02ff67c`, fails the
current independent native check because the result never moves to its final
bounds (exit 1). These checks do not merely mirror the added branch.

The first wide-window fixture fails its explicit over-height precondition;
the small-window Release case passes. New `OWNER_DOWN_ACK`/`OWNER_UP_ACK` lines
then exposed a substring collision in the old general `ACK` matcher: the shared
driver inspected the file witness before the later selection completed. Input2
anchors the full acknowledgment line. Production C inputs are unchanged between
input1 and input2. The final five workflows pass, retaining all preliminary
outcomes, without weakening the FD/geometry assertions or increasing deadlines.

The exact shared final-text screenshot was inspected at
`D:/NexaUI/out/picker-owner-reading-20261107.png`: the last escaped-name lines and
fixed focused Close are readable. Overlapping page geometry supplies the reading
path; this is controlled fixture evidence, not human screenreader/product-design
approval or an always-visible result summary.

## Frozen evidence and remaining scope

Bundle: `D:/NexaUI/out/nexaui-picker-reading-20261107.tar.gz`.
SHA-256: `8c4fe83d3fc8ce5c06be825e364bae55b57fe5e38238295e330e96f48789fcf2`.
27,366,530 bytes; 42,708 inventoried regular files, 8 links and 684 directories,
plus the manifest. The independent local verifier checks every file hash, size
and mode, directory mode and link target.

The collector binds 26 tested consumer inputs to the code archive with only
text CRLF normalization and the explicitly retained final README difference.
It checks 159 equivalent production SDK inputs, original model/archive hashes,
installed libraries, strict flags, JUnit, peer/host/runtime hashes, actual
selection identities, page acknowledgments, raw captures, IME barriers and both
negative outcomes. Inputs, builds, ordinary filesystem cases, preliminary failures
and exact previous native binary are retained in the bundle.

L11/I01/A02/Q02/Q04 advance within ordinary single-file/folder composition.
Native child-window parenting, actual OS close while a chooser is active,
multi/save variants, broader failures, service/grant adoption, other packet-6/7/8
surfaces, measured performance and physical/human acceptance remain open. The
earlier [AT-SPI peer heap incident](file-picker-directory-report.md#preserved-peer-heap-abort-incident)
is unresolved; these passing runs do not prove a fix. No 1.0.0 release.
