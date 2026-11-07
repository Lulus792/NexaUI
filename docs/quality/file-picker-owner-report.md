# Persistent owner and modal file/directory selection — 2026-11-07

Consumer source: `951e8d2aba147de6fe8c75f0077613380dadfd78`.
Installed NexaUI source remains `ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`,
version 0.1.23. The original Files model remains Aetheri `27de5cb`; the native
fixture uses the previously frozen Aetheri compositor from `d56d695`.
No SDK API/ABI, dependency, version, live Aetheri source or consumer pin changes.

The subsequent [keyboard-reading step](file-picker-owner-reading-report.md)
adds native evidence for a complete maximum escaped owner result. The executions
and remaining scope below describe this report's original source.

## Result and adoption boundary

The optional installed C17 [picker example](../../examples/32_aetheri_file_picker/README.md)
now supports `FP_OWNER=1`. A persistent Selection workspace owns an editable note,
Choose file/Choose folder controls, the last accepted selection and Close.
The chooser uses the existing `nx_modal_begin`/`nx_overlay_close` APIs for a
full-viewport modal **inside the same native window**. It is not a native child
window and supplies no separate-toplevel parenting contract.

The owner controls remain registered behind the modal. Existing SDK overlay
policy rejects their focus, actions and EditableText operations. Retired native
references also remain refused after the view/context changes. Cancel/Escape
restores the triggering button while retaining the note and last accepted
selection. Each reopening gets a monotonically increasing view epoch and fresh
editor; READ observes the same worker's current held folder and committed
filter. Unapplied picker drafts belong to their view. A late completed Pick
from a cancelled view is disposed, including its owned descriptors, rather than
replacing the owner's result.

An accepted file/folder result transfers its ordinary descriptor pair once.
The owner copies the original escaped display label and closes the preceding
pair only when successfully replacing it. Cancellation does not clear the last
selection. The last pair remains owned until replacement or owner destruction.
This composition follows the existing native host's `fstat` validation practice
on the UI thread; it does not establish a filesystem metadata latency budget.
These descriptors are neither permission grants nor durable storage identities.

One original-model worker survives all chooser views. Closing a chooser does not
Join it. Final teardown retires the context/window, releases owner resources,
then stops/joins/disposes worker work. The example uses public controls, fonts,
clipboard, IME, theme, focus, semantics and software-presentation contracts.
The picker semantic parent is configurable, with its standalone default of 1.

An independent Aetheri app can reuse this composition around an ordinary
file/folder selection on the existing installed SDK. Actual app adoption,
service authority and approved product design stay with their external owners.

## Executed checks

Fedora 44 VM, Linux 6.19.10, GCC 16.2.1, CMake 4.3 and Python 3.14.3; real
Liberation Sans, the frozen three-output compositor and private AT-SPI runtime.
Three installed profiles use `-Wall -Wextra -Wpedantic -Wconversion -Wshadow
-Werror`: static Release, shared Release, and static ASan/UBSan with active leak
detection and no suppressions. Each passes **3/3 CTests**, without skips:

- The existing actual-filesystem worker groups, including current held-directory
  selection and `wrong_thread=0`.
- The existing real-font picker correlation, keyboard, count, recovery and
  reflow checks.
- A new real-font owner test covering genuine modal ancestry/background blocking,
  note preservation, cancellation with a real queued old-epoch Pick completion,
  stale reply disposal, once-only transfer, file-to-empty-folder replacement,
  both preceding descriptors closed, focus restoration, epoch exhaustion and
  final descriptor closure after context destruction.

All **20 current-source native workflows** pass with host/peer exit 0, no forced
cleanup and no host/peer ASan/UBSan/LSan diagnostics:

| Group | Cases | Observed scope |
| --- | ---: | --- |
| Owner | 5 | Release light/controlled IME, Release dark, shared German 200%/360x300/high contrast/4,096 entries, matching sanitizer case, sanitizer held Refresh |
| Directory | 4 | Release/shared/sanitizer selection and light Cancel; filtered-empty current-folder selection and held-path replacement |
| Mixed input | 3 | Queued Tab/pointer/text/row bursts with original ordering checks |
| Recovery | 3 | Original Retry/Refresh, unavailable counts and native compact/resize recovery |
| Base picker | 5 | Release/shared/sanitizer maximum-entry selection, held-work Cancel and light Cancel |

The independent owner AT-SPI peer is not linked against NexaUI. It checks fresh
and retained background controls, original note text, row selection, retired
picker references, the complete result label, restored trigger focus and the
remembered child folder on reopening. Each ordinary owner run has eight
assistive stages; the held variant has nine. The driver compares both accepted
results against actual filesystem device/inode values and observes that the
old regular-file witness is absent from `/proc/<host>/fd` after replacement.
The owner reports exactly two accepted results through the subsequent Cancel.

In the held variant, original Refresh is explicitly stopped before chooser
Cancel. The owner remains usable and closes while Refresh is still held. Raw
logs prove `ORIGINAL_REFRESH_HELD < OWNER_VIEW_CLOSED < GUI_GONE <
ORIGINAL_REFRESH_RELEASED`; worker completion/Join follows GUI retirement.
There is no arbitrary-filesystem shutdown deadline.

The light owner case exercises controlled native text-input-v3 preedit, committed
`é`, a second preedit and actual Shift-Tab focus-loss cancellation. The resulting
`owner drafté` remains intact across all chooser views. Before another injection,
the driver observes an actual host frame and matching client commit count / server
processed serial (2/2, 4/4, 5/5). This is a deterministic protocol producer,
not installed-IME or human screenreader acceptance.

Two deliberate current-source defects both fail the core **and** native checks
with exit 1: replacing the modal with a plain semantic dialog exposes background
input; omitting old witness closure retains the replaced regular-file descriptor.
The core test checks both old descriptors; the native driver independently checks
the regular-file witness. No production guard/assertion or deadline was weakened.

## Preserved preliminary outcomes

The first new test used a pointer/length call where the public text-value API
expects `nx_utf8_span`; input2 corrects only that test call. A finalization helper
completed all three builds and 3/3 CTests per profile, then failed looking for a
peer compilation template in the initial failed receipt. Separate recorded peer
commands compile successfully; the entire helper is not reported as successful.

`pko1-release` passes with the retained preliminary peer. `pko6-light` fails when
second preedit is sent before the producer processes the latest client commit
serial. Input7 adds the actual-frame/processed-commit barrier described above;
`pko7-light` passes. These outcomes are retained and do not imply an SDK or
compositor production change. A home-directory capture helper upload also failed
with ENOSPC and left an unusable partial file; the valid private `/tmp` helper
and receipts are included instead.

The exact German/200%/small/high-contrast owner capture was visually inspected:
`D:/NexaUI/out/picker-owner-20261107-shared.png`. Choose file/folder and fixed
Close are readable; the owner body scrolls, the note reaches the body edge and
the result label is below the fold. Native long-result reading, always-visible
selection feedback and final Aetheri design are not accepted by this screenshot.

## Frozen evidence and remaining scope

Bundle: `D:/NexaUI/out/nexaui-picker-owner-20261107.tar.gz`.
SHA-256: `0c99e2024867bbc6f78cb3c2b552abe9be06fa3bfc2d5194dcef904fd3bc9282`.
32,270,608 bytes; 113,309 inventoried regular files, 14 links and 1,270
directories, plus the manifest. The local copy is checked by full file
hash/size/mode, directory mode and link target.

The collection checks all 26 tested consumer inputs against the exact code
archive with text CRLF normalization; the final README is the only permitted
documentation difference and both originals are retained. It also verifies
159 production SDK inputs against the installed implementation, accounting for
the already documented root async CTest registration, the five original model
inputs/full archive, installed library hashes, JUnit, native commands/binaries,
result identities, IME barriers, capture hashes and both counterexamples.

The bundle retains all input/code archives, builds, sources, ordinary filesystem
cases, preliminary failures, command receipts and a separately verified nested
archive of the three current raw PPM captures. Loose current/older expansion PPM
duplicates were unlinked only after full byte/size verification against frozen
guest/local archives; exact receipts are included. Sources, binaries, logs,
unique evidence, frozen archives and live Aetheri data were preserved.

D07/L10/L11/I01/A02/A06/P05/Q02/Q04 and scoped T04/T11 advance. This does not
complete packet 6. The [earlier peer heap-abort incident](file-picker-directory-report.md#preserved-peer-heap-abort-incident)
remains unresolved; this passing campaign is not a fix. Native child-window
parenting, actual OS-toplevel close while a chooser is active, owner long-result
reading, multi/save selection, wider I/O/permission/allocation failures, active
IME resize, real service/grant adoption and the other packet-6/7/8 surfaces stay
open. Physical/human acceptance remains separate and paused where requested.
No 1.0.0 approval or release is claimed.
