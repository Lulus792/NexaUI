# Picker read recovery and native resize — 2026-11-07

Consumer source: `f5a5d678fef7654671516e3e52e56f25ff8ff428`.
Installed SDK implementation remains
`ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`, version 0.1.23. The
[ordinary picker](../../examples/32_aetheri_file_picker/README.md) continues to
use five hash-checked original Files model/grapheme inputs from Aetheri
`27de5cb228da0f37264fad64d964ad7a660d210d`. The
[first native report](file-picker-native-report.md) and
[worker report](file-picker-worker-report.md) retain their exact earlier scope.

This changes an optional consumer, not the SDK API/ABI/version or Aetheri's live
Files application, source, launchers, defaults or pin. The example now links
system `libm` directly for its own geometry; no third-party UI/runtime dependency
is added to NexaUI. No normal application adoption or 1.0.0 release is implied.

## Original read/Enter recovery

An initial failed read reports that no folder was loaded. A failed Enter retains
the previous complete observation and reports that fact. Retry is now an explicit
action beside the error, retaining the failed original command by value. It
repeats that READ/Enter/Refresh using the worker's current state token rather than
silently refreshing the retained parent. Original ordinal/inode/held-FD checks
still apply. Malformed/unknown completion recovery observes with READ instead of
replaying a possibly completed operation.

A changed selected inode requires explicit Refresh folder before a new selection
can succeed. The original 64-level path boundary offers Up rather than a pointless
repeat of the same Enter. Recovery actions receive safe focus beside their
message. PageUp/PageDown use half the actual body viewport; scrolling an error
does not continually snap the focused recovery button back into view. The message
can be read while retaining its keyboard focus, then scrolled back for activation.
Counts and observations are not grants or operation receipts.

## Measured layout and resize

Control/footer text is measured using the existing real-font wrap helper, cached
by width, text size and locale. The normal view remains available at the previously
usable 180x160/16-pixel text configuration. If normal controls cannot fit, a compact
scrollable size hint and Cancel remain available. Very small layouts use an
accessible close-icon Cancel; the narrow footer does not consume the space needed
to read the first hint line. Folder, page, selection and unapplied query draft
remain separate from geometry.

The bottom-right grip uses a genuine native pointer press and the existing
`nx_window_begin_resize` API. It restores the prior keyboard focus rather than
letting a nonfocusable grip discard it. Switching normal/compact mode retires
native publication/context and unposted old-mode activation intents; a safe focus
returns on restoration. Accepted worker work remains independent. Resizing itself
posts no model operation.

The controlled runner records Wayland client traces. It waits for the pinned
compositor's resize-start and release configures before moving on; a flushed
client request is not treated as compositor admission. Its first ordinary window
origin is (40,40), as defined by that frozen compositor. This is private virtual
input and software presentation, not physical display/seat acceptance.

## Executed checks

Fedora 44 guest, Linux 6.19.10-300.fc44 x86-64, GCC 16.2.1, CMake 4.3.0,
Python 3.14.3. Static Release, shared Release and static Debug ASan/UBSan/LSan
each compile under `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`
and pass **2/2 CTests**, with no skips. Their explicit checks remain active under
NDEBUG. They retain the six original worker groups and prior real-font cases,
then add actual filesystem/UI assertions for:

- A failed Enter into 4,097 entries, retained parent observation and a Retry
  that enters the same directory after it reaches 4,096.
- An initial 4,097-entry READ failure with no prior observation, followed by
  READ Retry admitting all 4,096 remaining entries.
- Geometry through 360x300, 240x160, 64x64, 1x1 and back; normal 180x160 is
  retained at 16-pixel text. Query draft, model state, page and selection survive.
  Compact hidden Open refuses actions; an unposted old-mode Pick is retired.
- All 64 original held levels, refusal of level 65, the specific path-limit
  message, no Retry for that unchanged Enter, and ordinary scroll without
  forced focus reveal.

Three independent native recovery cases (`pkra-release`, `pkra-shared`,
`pkra-asan`) each pass **seven stages**. Shared and sanitizer cases use German,
200% text, 360x300 and high contrast. Every case starts with a real 4,097-entry
root, retries after one file is removed, fails Enter into another 4,097-entry
directory, reads the retained error, then retries into all 4,096 children.
Home/End reaches the final child. Real pointer drags visit 64x64 and 240x96 and
restore the original size, with unchanged model/page/selection and exact
unapplied draft. Old row/Open/editable-text references refuse actions even after
returning to the same geometry. A replacement inode is refused, then explicit
Refresh and a fresh selection yield the exact replacement device/inode and leaf.
The old inode remains saved separately in the test corpus, preventing accidental
inode reuse from weakening that check. Seven requests are issued; resize issues
none. All three hosts/peers exit 0.

Five current-source regressions (`pkra-base-*`) each pass the previous nine-stage
native workflow: static Release, shared small/German/200%/high-contrast,
sanitizer equivalent, sanitizer held Refresh/Cancel, and Release/light with no
IME protocol. Controlled preedit/cancel/commit and absent-transport editing keep
their separate scopes. Pending native Cancel still retires the GUI before held
original I/O is released and joined. All positive children close without forced
termination, and no accepted log reports sanitizer errors. Process maps retain
the software/Wayland-only presentation boundary.

Three isolated current-source counterexamples each fail core checks and the
applicable native workflow (Exit 1): parent Refresh substituted for Enter Retry;
compact layout bypassed below 180 pixels; and unconditional focus reveal after
user scroll. The initial **wide** native scroll mutant passed with zero reading
pages, so it did not exercise the defect. The targeted small/German/200% case
fails the independent error-top geometry check. Both outcomes are retained.

Initial failures are preserved: missing direct `libm` in the shared example,
an overbroad helper edit producing an invalid CMake command, early pointer motion
before compositor resize admission, and a reading test that assumed one PageUp
was sufficient. The final reading test derives bounded page counts from native
geometry. No prior deadline was increased, no assertion weakened, and no warning
or sanitizer suppression added. Visual review found and corrected the clipped
first hint line in the flat 200% window. Inspected final images are
`D:/NexaUI/out/picker-recovery-20261107-final-compact.png` and
`D:/NexaUI/out/picker-recovery-20261107-error-top.png`; long messages intentionally
continue through the scroll area. This is automated-fixture visual review.

## Frozen evidence

Bundle: `D:/NexaUI/out/nexaui-picker-recovery-20261107.tar.gz`.
SHA-256: `8b39629171b207b5989e3f57a3466261429ef3887ca7cbd9e8c023d48efc1a1e`.
Size: 43,794,525 bytes; 310,547 regular files, 19 symbolic links and 2,713
directories. The copied bundle is independently checked using full hashes,
sizes, file/directory modes and link targets, including raw non-UTF-8 names and
deep empty directories. It contains 1,074 source provenance records, code/input
archives, source/builds, actual filesystem cases, installed artifacts, JUnit,
commands, native results/traces and counterexamples. Production SDK equivalence
covers 159 inputs, accounting explicitly for the historical extra root async
CTest registration. Original model inputs are checked by raw hash; consumer
source comparison permits only text CRLF normalization.

Raw captures are retained in two nested, independently locally verified archives:
`nexaui-picker-recovery-preliminary-captures-20261107.tar.gz` (SHA-256
`4392548a7fece37a5edc97541876a11cba3b99b482a7df043d3528050cecc9e3`) and
`nexaui-picker-recovery-remaining-captures-20261107.tar.gz` (SHA-256
`6ad0e707760a1f3afc2bd30eabcf56e95b56f62370073cd958a607bfd6f8133c`).
After verification, 69 byte-identical loose PPM duplicates (969,754,773 bytes)
were retired to restore guest capacity; receipts are included. Raw image bytes
remain in the guest and local frozen archives. Do not rerun the collector over
retired loose inputs as though this were a fresh experiment.

## Remaining scope

This advances B02/W03/L02/L11/I01/I06/I07/A02/A06/A10/D07/P04/P05/Q02/Q04
within the ordinary picker. It does not close those catalogue-wide requirements.
At this revision, the unread-root status is explicit but the zero-valued count
label still needs an unavailable marker. The subsequent
[count-state step](file-picker-counts-report.md) addresses that scoped L11/D07 item.
Additional I/O/permission/allocation failures, unpaced mixed-input routing,
active-IME resize, broader scale/outputs/platforms and native deep/long-name
ancestor reading remain open. Directory/multiple/save choices, owner/modal
integration, richer file-type/sorting policy, real services/grants and normal
Aetheri adoption remain separate. Performance, physical/human acceptance and
final product design remain open, as do subsequent packet-6 compositions and
packets 7/8. Neither local tests nor the earlier main CI close that wider scope.
