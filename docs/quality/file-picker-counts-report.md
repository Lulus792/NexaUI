# Picker unavailable and empty observations — 2026-11-07

Consumer source: `f1da1ba0e98440c611a671b6127e0abaf240a914`.
This follows the [read recovery and native resize](file-picker-recovery-report.md)
step. The installed SDK remains `ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`
(0.1.23); the original Aetheri Files model remains
`27de5cb228da0f37264fad64d964ad7a660d210d`. No SDK API, dependency, Aetheri
live source/default/pin or normal application adoption changes.

The ordinary picker previously showed `0–0 of 0 entries (0 total)` before any
successful read and after a failed initial read. That incorrectly suggested an
observed empty directory. One shared formatter now supplies both real-font
layouts and semantic names: **Entries not available / Einträge nicht verfügbar**
until the worker has admitted an observation. A successful empty observation
still has numeric zeros. A filter without matches retains the original total;
later failures retain the counts of the preceding complete observation. Busy
status continues to describe pending work separately. Counts confer no authority.

## Executed checks

Fedora 44/Linux 6.19.10 x86_64, GCC 16.2.1 and CMake 4.3.0; caller-selected
Liberation Sans, installed Wayland/software SDK. Static Release, shared Release
and Debug ASan/UBSan/LSan each pass **2/2 CTests**, no skips. The example's
`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` remains enabled.

The real-font test checks semantic count names before reading, during an initial
pending read, after a real failed 4,097-entry initial read and after its successful
Retry. It also checks a successful no-match filter, an actual empty directory,
and its retained zero counts during and after Refresh fails when 4,097 files are
added. Both locales are covered. Existing original worker, ticket/key, paging,
escaped-label, Retry, path-limit and compact-size tests remain passing; original
model wrappers report `wrong_thread=0` in all three profiles.

Three independent native AT-SPI recovery cases pass: English/static Release,
German 200%/360x300/high-contrast/shared Release and the sanitizer equivalent.
Each retains seven externally coordinated stages and three real native resize
phases, adding two real filter requests between initial Retry and failed Enter.
The peer requires the unavailable label after the initial failure, refuses a
numeric empty label there, and checks successful filtered empty counts with the
original total of 4,096. It then clears the filter and completes original Enter
Retry, error reading, last-row selection, resize/draft/focus preservation, inode
replacement refusal, explicit Refresh and exact fresh file-witness selection.
Each host closes normally with nine requests; no positive child needs forced
cleanup and no accepted log contains sanitizer diagnostics.

The same current tests built with the unchanged `f5a5d67` picker presentation
reject in **core and independent native checks, both Exit 1**, at the missing
unavailable count name. The first outcome-verifier helper mistakenly expected
the macro spelling `CHECK(node)` in the native log; the existing macro prints
the expression `node`. A separate verifier matches the actual failing source
line and both recorded exits without changing or rerunning the tests. This
helper failure and correction remain in the bundle.

This step checks text/state behavior and native semantics, with prior rendering
and resize evidence retaining its original revision. It adds no new visual,
installed-IME, human screenreader, physical-output or performance acceptance.

## Frozen evidence and remaining work

Bundle: `D:/NexaUI/out/nexaui-picker-counts-20261107.tar.gz`.
SHA-256: `2d114c185eeca6ef9fd1ba6939cdb264b6947b043334d03f2a6397073855035f`.
25,776,007 bytes; 58,638 regular files, five links and 635 directories. Full file
hash/size/mode, directory-mode and link-target verification is performed on the
local copy. The bundle contains exact code/input archives, tested source,
installed libraries, original inputs, build commands/artifacts, JUnit, actual
filesystem cases, native traces/results and the original-presentation negative.
Compiled inputs match the code archive with only text CRLF normalization;
159 production SDK inputs remain equivalent to the installed implementation,
with the previously documented root async CTest registration accounted for.

The guest was nearly full. Before new builds, 11 strictly named old experiment
`files` corpora were compared by full file hashes/sizes/modes against the already
independently verified recovery bundle, then retired as loose duplicates. Source,
logs, binaries and the frozen guest/local archives remain. The included receipt
names every retired root; this is not deletion of unique evidence or live data.

L11/D07/A02/Q02 advance within this picker; the full catalogue remains partial.
Unpaced mixed-input routing is the next input issue: the native host currently
intercepts row commands before queued preceding focus/pointer events have been
applied. Additional permission/I/O/allocation failures, active-IME resize, long
native ancestor reading, directory/multiple/save variants and owner/modal
integration remain open. Actual service/authority/durable reconciliation, other
packet-6 compositions, packets 7/8, performance and physical/human acceptance
remain open. This is not 1.0.0 approval.
