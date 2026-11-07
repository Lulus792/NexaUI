# Picker held-directory selection — 2026-11-07

Consumer source: a040d1288106cdbb6f1afaf623fb3a98200ea96c.
Installed SDK implementation remains
ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36 (0.1.23). Original Aetheri Files
model: 27de5cb228da0f37264fad64d964ad7a660d210d, with the same five raw-hashed
model/grapheme inputs. Only the optional consumer changes; no SDK API/ABI/version,
runtime dependency, live Aetheri application/default or consumer pin changes.

## Selection contract

FP_DIRECTORY selects one current held folder. Enter still navigates a directory
row; a regular-file row cannot complete this mode. The fixed action explicitly
says “Choose this folder / Diesen Ordner auswählen”. It is available after an
admitted original observation, including the caller's root, an empty folder and
a filter with no matches. It remains unavailable before a successful initial
read. Normal and compact help refer to folder selection.

The existing model already exposes ae_files_directory, which returns an owned
CLOEXEC duplicate of its current held directory. The worker uses that contract
and creates a second independent CLOEXEC duplicate, returning both with literal
leaf dot. Both describe the same directory; neither is a reconstructed display
path. Renaming the held directory and replacing its old pathname cannot redirect
the choice. A failed second duplication closes the first descriptor. Stale
adapter state is rejected before acquisition; accepted attempts advance the
existing state token. All original model calls remain on the worker.

The caller uses the existing once-only fp_reply_take, or message disposal closes
unclaimed descriptors. GUI teardown, Stop and Join keep their existing separate
lifetimes. These ordinary process descriptors confer no grant, durable identity,
allocation, mutation or service authority. This example reads no selected content.

## Executed checks

Fedora 44/Linux 6.19.10 x86_64, GCC 16.2.1, CMake 4.3.0 and the installed
Wayland/software SDK. Static Release, shared Release and Debug ASan/UBSan/LSan
each pass **2/2 CTests**, no skips, with strict warnings including
-Wconversion -Wshadow -Werror.

The worker test now includes an actually empty root, original current-directory
selection, stale-token refusal without descriptors, held-child rename/path
replacement, exact device/inode/type checks, CLOEXEC, once-only transfer, 64
unclaimed-response disposal cycles, abandoned queued completion disposal and
descriptor usability after worker destruction. Descriptor counts balance;
original model wrappers report wrong_thread=0. Existing worker groups remain.

The real-font view test checks unread refusal, the German action name at
200%/360x300, regular-file Enter refusal, successful selection from a no-match
view, and folder-specific compact help at 64x64 with hidden action refusal and
restored availability after returning to normal size. Existing correlation,
counts, Retry, paging, escaped-label and path-limit checks remain passing.

Four native directory workflows pass: English/static Release; shared
German/200%/360x300/high contrast; sanitizer equivalent; and Release/light Cancel.
Each independent AT-SPI peer navigates into a real folder, applies a no-match
filter, checks the enabled folder action without a selected row, refuses old
editable references and observes the retained folder after rename/replacement.
Selection returns the exact held directory's kernel device/inode and dot leaf,
rather than its replacement's identity. Cancel returns no selection. These
directory cases use ordinary editing without the text-input protocol.

Eleven current-source native regressions also pass: three mixed-burst workflows
(including two full 4,096-entry cases), three original Retry/count/resize
workflows, and five baseline IME/Cancel configurations. Controlled preedit,
focus-loss cancellation and commit retain their separate baseline scope. Held
original Refresh still completes after GUI retirement. All 15 accepted
hosts/peers close normally with no forced cleanup or accepted sanitizer
diagnostic. The heap-abort incident below remains separate and unresolved.

Two shipping-source counterexamples each fail core and independent native checks
(Exit 1): duplicating the initial worker root instead of the current held folder,
and disabling folder choice when no row is selected. The former reaches normal
native completion but returns the wrong directory identity; the runner rejects
the actual witness. The latter fails the empty-filter action check.

The exact final German/200%/360x300/high-contrast capture was inspected:
D:/NexaUI/out/picker-directory-20261107-shipping.png. Both fixed actions have
fully readable labels; the body intentionally scrolls and its offscreen controls
can be partially clipped at the viewport boundary. This is scoped fixture
visual review, not final product design or human screenreader acceptance.

## Preserved peer heap-abort incident

The first final-source shared recovery regression aborted in the ordinary
independent peer after root Retry, reporting glibc “corrupted size vs. prev_size”.
The host had admitted the complete 4,096-entry observation. The peer core shows
the allocator detecting corruption while the private libatspi consumes accessible
interfaces during discovery. This locates detection, not the originating write;
it does not establish that the library, peer or SDK caused the corruption.

The failed run, core, stack, exact original peer library and command are retained.
The same workflow passes with an ASan/UBSan peer against that original library,
with a separately ASan/UBSan-built upstream AT-SPI 2.60.7 library, and on an
ordinary repeat using the unchanged original peer/library. No assertion, timeout,
cache behavior or sanitizer check was disabled. These passes **do not prove the
incident fixed**; reliable assistive acceptance and its root cause remain open.

The additional library is solely a private diagnostic tool for the OS peer.
The [upstream 2.60.7 source](https://github.com/GNOME/at-spi2-core/tree/2.60.7)
archive is hash-recorded; its build uses existing toolchain/runtime libraries and
owned private pkg-config metadata. The first configuration lacked libxml2
metadata; the resumed configuration points to existing system headers/library.
Only the diagnostic peer loads this library. NexaUI and Aetheri acquire no
dependency or global configuration change. An initial diagnostic command also
looked in the earlier build receipt rather than the shipping receipt; that
orchestration lookup was corrected before compiling the diagnostic peer.

## Frozen evidence and remaining scope

Bundle: D:/NexaUI/out/nexaui-picker-directory-20261107.tar.gz.
SHA-256: e51195d1975cb1982700036930ea53d1947ad2befd73f1a6cd8eb554e1291d54.
40,472,641 bytes; 216,755 inventoried regular files, 35 links and 2,214
directories, plus the manifest. The local copy is checked by full file
hash/size/mode, directory mode and link target.

The bundle includes exact code/three input archives, tested source, original
model inputs/archive, installed libraries, builds/JUnit/commands, actual native
and core filesystem cases, raw captures, both counterexamples and the failed
peer/core plus diagnostic sources/builds/results. Compiled consumer inputs match
the code archive with only text CRLF normalization. The final README is a
documentation-only materialization after execution. 159 production SDK inputs
remain equivalent to the installed implementation, accounting for the previously
documented root async CTest registration.

Several older loose experiment corpora were retired only after full comparison
against their already independently verified frozen guest/local bundles.
Receipts identify each exact root and archive. No unique evidence, source,
logs/binaries, frozen archives or live Aetheri data were removed. Preliminary
candidate results retain their input archives/logs; shipping results are bound
to the retained shipping binaries.

D07/L11/I01/A02/A06/P05/Q02/Q04 advance within ordinary file/directory selection.
Owner/modal integration, multiple/save selection, broader I/O/permission/fault
coverage, real services/grants and normal Aetheri adoption remain open. So do the
peer heap incident, active-IME directory/resize cases, native long/deep ancestor
reading, sustained input/performance, wider platform/output configurations,
physical/human acceptance and the other packet-6/7/8 surfaces. No 1.0.0 approval.
