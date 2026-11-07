# Logical multi-picker and owned result pages — 2026-11-07

Consumer source: `c04d94292df0ee9c3afa7a1b6ae50c61a05fb12b`.
Installed NexaUI remains `ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`, version
0.1.23; original Files remains Aetheri `27de5cb`. This composes the existing
[atomic worker batch](file-picker-batch-report.md) with public logical selection
and the [persistent modal owner](file-picker-owner-report.md). No SDK API/ABI,
dependency, live Aetheri source, consumer pin or 1.0.0 change.

## Behavior and adoption

The owner now offers Choose files alongside its single-file/folder controls.
Mode 2 of `fp_owner_open` creates a fresh multi-picker; an embedding consumer can
set `picker.multiple` before its first request/frame. The native demonstration
also accepts `FP_MULTIPLE` for its standalone path. The executed native acceptance
below covers the owner composition; the standalone path is built in all profiles.

The picker binds `nx_selection_model` to every regular file in the complete
observed catalog. Off-page members are published with their original full labels
and offscreen state, so a page change cannot silently reduce a selected set.
Directories are separate navigation buttons. Press/click, Space or Enter opens
them through the original worker; Up returns through its held-directory stack.
No second filesystem/filter/sort model or path reconstructed from labels is used.

Arrow/Home/End moves active focus independently of selection. Space toggles a
file, Shift navigation/click extends from the anchor, Ctrl-click toggles and a
plain click replaces the set. Ctrl+A selects all observed regular files. Native
Selection item/bulk operations use the same logical model. After successful
end-frame, changed selection triggers a repaint before presentation; a same-frame
pending Open is discarded when its selected set changed. Catalog replacement
clears selection instead of reusing ordinals for new objects. Matching retained
observations preserve it. Old native publications retire on requests/results and
page changes, including return to an earlier page.

The owner takes the complete descriptor set once, retains one bounded immutable
reply for original raw names/display labels, and closes every preceding result
only on successful replacement. Cancel preserves the note and accepted result;
reopening starts a fresh empty logical selection. Result pages retain at most
16 name layouts, preserve the complete owned set and restore a valid page button
after retiring native publication. PageUp/PageDown reads the result body while
Close stays fixed. The note editor and active modal retain their own keys.

These are ordinary-user file descriptors, not grants or durable IDs. Owner
validation follows the earlier host's UI-thread `fstat` practice; filesystem
metadata latency has no accepted budget. Retaining one whole copied reply costs
up to the worker's 5,539,472-byte logical payload, plus the actual descriptor
record array and layout/font/framework allocations. This is bounded ownership,
not a process-RSS or energy/performance acceptance claim.

## Executed checks

Fedora 44 VM, Linux 6.19.10, GCC 16.2.1, CMake 4.3, Python 3.14.3, real
Liberation Sans and the frozen original Aetheri `d56d695` compositor with three
virtual outputs. Static Release, shared Release and static ASan/UBSan with active
leak detection each pass **4/4 CTests**, with no skips/suppressions and strict
warnings as errors. Existing worker/view/owner suites remain, and the new
real-font multi-picker suite checks:

- Directory Press/Up through actual original model work, including a genuinely
  empty child, then complete regular-file membership.
- Space, cross-page End, Shift range, Ctrl+A, deselection and native semantic
  Select/Add/Remove with complete selected IDs and offscreen selected semantics.
- Actual once-only batch adoption, note/focus, Cancel/reopen, batch-to-file and
  file-to-batch replacement, all prior descriptors closed, three result pages
  and final closure of all 80 descriptors from a forty-file result.

All **ten final native workflows** pass with host/peer exit 0, no forced cleanup
or ASan/UBSan/LSan diagnostics:

| Native scope | Cases | Evidence |
| --- | ---: | --- |
| Ordinary multi-owner | 4 | Release, maximum-entry shared/sanitizer German 200%/360x300/high contrast, held-Refresh sanitizer |
| Explicit input-adapter variant | 1 | Actual native input with a test-only wrapper exposing already-released modifier state on Control key-up |
| Existing single-owner/reading | 5 | Release, maximum shared/sanitizer, controlled-IME light and held-work sanitizer |

The independent AT-SPI client has no NexaUI linkage. Multi-owner runs have
15 assistive stages (16 with held work). They check original folder button/Up,
keyboard selection across page retirement, refusal of old item and whole-owner
Selection references, logical native SelectAll of 40 or 4,095 regular files,
clear/add operations and an exact seventeen-file result. Native Ctrl-pointer
bursts stop only the owned host while submitting press/click/release, then
observe actual selection before proceeding. Source labels and actions remain
scoped to their actual native identities.

Both owner result pages are read in overlapping half-viewport steps with fixed
Close focus, reaching the actual final label bounds. The desktop's second page
is already fully visible and needs zero scrolling; that complete visibility is
checked explicitly. The driver validates all seventeen witness device/inode
values through `/proc/<host>/fd`, retains the exact set through Cancel, then
observes every old file witness gone after a different single-file result.
The owner accepts exactly two results. Held work proves `REFRESH_HELD <
OWNER_VIEW_CLOSED < GUI_GONE < REFRESH_RELEASED`, with Join after GUI retirement.
Native logical SelectAll does **not** mean acquisition of 4,095 FD pairs: the
actual accepted native batch has seventeen files; descriptor-limit refusal is
covered separately by the worker suite.

The final German/200%/small/high-contrast reading screenshot was inspected at
`D:/NexaUI/out/picker-multiple-20261107-final-reading.png`: the last original filename and
fixed focused Close are readable. This is controlled fixture review, not final
Aetheri design or human screenreader/physical-device acceptance.

## Counterexamples and retained diagnostics

Omitting off-page projection fails the core selection/frame check and the native
complete-catalog query, which refuses the incomplete initial publication before
its first acknowledgment. Omitting owned-batch FD closure fails the core FD
assertion and independent native `/proc` witness check. All four failures exit 1.

The modifier-boundary mutant **passes on the ordinary frozen producer** and is
retained as a positive control. With the explicitly labeled test-only adapter
variant (same actual native key/pointer events, released modifier bit on key-up),
the unchanged host passes and the mutant fails its exact Ctrl-toggle assertion
(exit 1). This supports that bounded dispatcher ordering contract; it is not a
claim that the original compositor/adapter had a demonstrated production defect.
There is no artificial core test standing in for this host-only event boundary.

Maximum-size publications trigger the installed Linux SDK's conservative
**4 MiB bulk `Cache.GetItems` reply budget**. The private AT-SPI client logs the
explicit limits error; individual Accessible/Selection operations still complete
in these workflows, as the SDK's existing lazy-query contract allows. Every
warning remains in the bundle. No production guard, cache policy, reply limit or
deadline was relaxed. Human-reader behavior and bulk-cache/performance acceptance remain
open. This is separate from the earlier unresolved
[ordinary peer heap-abort incident](file-picker-directory-report.md#preserved-peer-heap-abort-incident);
these successful runs do not prove that incident fixed.

Preliminary inputs/results are retained: the initial modifier constant typo;
the owner focus-restoration failure before publication retirement; a driver local
`stat` variable shadowing its expected-child set after fifteen completed stages;
the over-height assumption for an already-visible desktop result; and mutant
orchestration that stopped before mutation on a production-build name collision.
The final mutant commands all have their intended exits. Its concluding text
lookup incorrectly expected a later native end-frame failure; a separate verifier
records the actual earlier incomplete-catalog boundary without rerunning or
weakening the checks. Owned source-copy mtimes were normalized after future-mtime
build warnings; system clocks and frozen evidence were unchanged.

## Frozen evidence and remaining scope

Bundle: `D:/NexaUI/out/nexaui-picker-multiple-20261107.tar.gz`.
SHA-256: `80242cbff915b03003961c31a8086a535b7f8d3898b490eeaa2e2ce0e81c1da1`.
56,591,611 bytes; 148,149 inventoried regular files, 32 links and 2,144 directories,
plus the manifest. The independent local verifier checks every file hash, size
and mode, directory mode and link target.

The collector checks all 29 current consumer inputs against the exact code
archive, text CRLF normalization, 159 unchanged production SDK inputs, original
model/archive/library hashes, four JUnit cases per profile, native commands and
actual binaries/runtime, full set/witness identities, page reading, images, IME
barriers, held-work order and every targeted negative/control outcome.
Earlier native bytes are retained separately from final binaries. Fixtures and
mutants use owned private temporary space. Fifteen older loose PPM duplicates
were retired only after full hash/size/mode verification against already frozen
guest/local archives; receipts and both inventory attempts are included.

L05/L11/I01/I07/A02/A03/A06/D07/P05/Q02/Q04 advance within this ordinary multi-owner
composition. Save variants, standalone native multi-picker acceptance, native
child-window parenting/OS close while a chooser is active, active-IME multi-view
resize and broader permission/allocation/I/O failures remain open. So do real
services/grants and Aetheri adoption, device/software/trusted system compositions,
packet-7/8 work, declared performance budgets and physical/human acceptance.
The full 103-ID goal remains active; no whole-catalogue or 1.0.0 approval is claimed.
