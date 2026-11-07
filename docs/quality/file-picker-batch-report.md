# Atomic original-file selection batches — 2026-11-07

Consumer source: `44a185532bd9ab02895a465b6c67f23dbb08e591`.
Installed NexaUI remains `ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`, version
0.1.23; original Files remains Aetheri `27de5cb`. No SDK API/ABI, dependency,
live Aetheri or pin changes. This advances the worker foundation for a complete
multi-picker; **the visible picker still selects one file or folder**.

## Contract and ownership

The existing worker accepts `FP_PICK_MANY` with a copied 4,096-bit set of
complete-snapshot ordinals and the exact observed adapter state. A bitset has
no duplicate/order ambiguity. Empty sets are rejected before submission.
An admitted attempt supersedes older commands once, including on failure.
Nonexistent/filtered-out ordinals, directories and symlinks cannot become
regular-file results. The worker reuses original model visibility, selection
and held-directory/inode-checked `ae_files_open_selected` calls. It never opens
a displayed label or rebuilds filenames from UI text. Ordered matching scans
the original visible catalog once, without a separate sort/filter model.

Success contains the complete descriptor set in original snapshot order;
corresponding raw leaves and escaped labels remain in its immutable rows.
`fp_reply_take_many` transfers the complete set once to a separate caller buffer.
Insufficient capacity clears the output count and retains ownership for retry or
disposal. Any acquisition failure closes the entire already acquired prefix
and returns no partial result. Unclaimed messages and abandoned queued results
close every pair on disposal. Existing single-file/directory paths remain.

This is an atomic **result and ownership** contract. The per-file witness calls
are sequential; no atomic filesystem transaction, grant, service admission or
durable storage identity is implied. Ordinary OS descriptor limits can reject a
large set. The adapter neither raises limits nor truncates a failed set into
success. All original model and per-file witness acquisition calls stay on the
worker.

The descriptor table/count add 65,544 fixed bytes per reply on the tested Linux
x86-64 build: header 132,752 bytes, row 1,320 bytes, maximum logical reply
5,539,472 bytes. Commands add a 512-byte mask. The existing full bounded reply
reservation precedes original model work, then attempts shrink to actual rows;
there is no new allocation between the first and last witness acquisition.
The two-slot channel and notification/Stop/Join ownership remain unchanged.

## Executed checks

Fedora 44 VM, GCC 16.2.1, CMake 4.3, Python 3.14.3 and the same installed static
Release, shared Release and static ASan/UBSan/LSan profiles. All strict warnings
remain errors. Each passes **3/3 CTests**, no skips or suppressed diagnostics.
The actual worker suite now has eight groups, including:

- Complete order and duplicate collapse from the bitset; exact original inode
  witnesses, CLOEXEC, capacity refusal/retry and once-only transfer.
- Actual mid-batch inode replacement and an injected second original-open I/O
  failure after a real first acquisition, both with complete FD balance.
- Stale adapter state, directories/symlinks, nonexistent and filtered-out
  ordinals, including successful complete-snapshot ordinal 4,095.
- All 4,096 bits against a temporary 64-FD soft limit in this isolated test
  process: explicit failure, no truncated result, all acquired FDs closed, then
  the original process limit restored. This is not success opening 4,096 pairs.
- Disposed caller-held batches and a completed unreceived batch abandoned during
  worker destruction; existing original navigation, wakeup, backpressure,
  GUI-before-worker teardown and single-file/folder tests remain.

`wrong_thread=0` covers the wrapped original model calls. The measured Release
suite reports 74,844 such calls, including the original 4,096-entry workload.
Current real-font picker and owner/long-result suites also pass.

Five current native **single-selection regressions** pass with host/peer exit 0
and no forced cleanup or sanitizer diagnostics: dark small Release, maximum-entry
shared and sanitizer German 200%/small/high contrast, controlled-IME light, and
held-Refresh sanitizer. They retain full long-result reading, note, focus,
file/folder replacement, stale references and GUI-before-worker teardown.
They do not exercise a native multi-picker or native whole-set transfer.

Two targeted Release counterexamples fail with exit 1. Clearing a failed batch's
count without closing its acquired prefix fails actual FD balance. Bypassing
the state guard only for `FP_PICK_MANY` fails the stale-state batch assertion.
The positive three-profile suite runs unchanged guards and active leak detection.

## Frozen evidence and remaining scope

Bundle: `D:/NexaUI/out/nexaui-picker-batch-20261107.tar.gz`.
SHA-256: `d1e83cfbddafeea890dbcf87c70106327170a65f23a968282bc7ccef019594f4`.
27,339,769 bytes; 46,758 inventoried regular files, 15 links and 785 directories,
plus the manifest. The independent local verifier checks all file hashes, sizes,
modes, directory modes and link targets.

The collector verifies the 26 tested consumer inputs against exact code with
text CRLF normalization and only the retained final README difference; 159
production SDK inputs remain equivalent to the installed implementation. It
also checks original model/archive and library hashes, strict flags, JUnit,
all five native command/runtime hashes, actual file identities, full-name page
reading/captures, IME barriers, held-work order and both counterexamples. The
bundle retains source/input/code archives, builds, actual filesystem cases,
commands, native captures and mutant sources/results.

D07/P05/Q02 advance within worker-owned result acquisition/disposal. The next
consumer work is to bind the existing public `nx_selection_model` APIs to the
complete logical membership, expose keyboard/pointer/native assistive whole-set
selection across pages and compose its full result with the owner. This remains
required; it is not replaced by the worker contract. Save variants, native child
parenting/active OS close, service/grant adoption, other packet-6/7/8 surfaces,
performance and physical/human acceptance stay open. The earlier
[AT-SPI peer heap incident](file-picker-directory-report.md#preserved-peer-heap-abort-incident)
remains unresolved. No 1.0.0 release.
