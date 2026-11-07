# Native single-file picker over the original Files worker — 2026-11-07

Consumer: `185389787c0d1e07fa259c823da3e25ad3e97475`.
Installed SDK: `ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`, version 0.1.23.
Original model: Aetheri `27de5cb228da0f37264fad64d964ad7a660d210d`, five
hash-checked model/grapheme inputs. The earlier
[worker report](file-picker-worker-report.md) records its independent ownership,
complete snapshot and descriptor contracts. This step adds the actual ordinary
Wayland/software [picker](../../examples/32_aetheri_file_picker/README.md).

No SDK implementation, public ABI, dependency or version changes. No live Aetheri
source, original Files application, consumer pin, launcher or default changes.
This optional consumer can be reviewed independently; it is not normal owner
adoption or a 1.0.0 release.

## Composition and behavior

The caller supplies a font and ordinary root directory. The original model owns
all directory enumeration, sorting/filter membership, held ancestor FDs and
inode/type checks on one worker. The UI only consumes complete copied results.
An editable name filter uses existing text-field/clipboard/IME APIs, explicit
Apply/Enter and a 255-byte validator. Hidden entries use the existing checkbox.
The expandable path exposes all held ancestors; directory rows have vector icons.
Opening a directory sends its complete-snapshot ordinal back to the worker.

The list displays explicit 16-row pages with full visible/total counts. All 4,096
admitted entries remain reachable; there is no truncated first-page result.
Row Up/Down and Home/End cross page boundaries, reveal/focus the requested row and
select it explicitly. Page buttons clear hidden selection and restore an enabled
neighbor when an endpoint button becomes disabled. PageUp/PageDown read long
scrolling content without changing focus. Text uses the existing emergency
grapheme-wrap helper; Open/Cancel remain outside the body and stack below 480
logical pixels. The default view is English/dark at 720x580; the checked small
configuration is German/200% at 360x300 with high contrast.

One request is active per view. Posting occurs after successful frame completion.
Both channel ticket and copied view/request must match before adoption. A malformed
current response becomes an error without replacing the last good observation.
The native host retires publication/context around requests, admitted responses
and page changes, including return to the same page. Pending old input, native
row actions and editable-text references cannot retarget a fresh view. Safe focus
is restored after replacement. The event loop waits on `nx_window_poll_with_fd`
and the worker wake pipe, without an animation or redraw timer.

Enter on a regular-file row or Open reuses the original selected-file check.
The demonstration takes the owned directory/O_PATH witness once, checks the
kernel file kinds, prints literal leaf bytes as hexadecimal plus device/inode,
then closes the FDs. It neither reads contents nor creates a grant, StorageRef,
installation receipt or durable file identity. An integrating caller can take
the same owned descriptors instead. Cancel yields no selection; Esc and the
ordinary Cancel button remain available.

## Executed checks

Fedora 44 guest, Linux 6.19.10-300.fc44 x86-64, GCC 16.2.1, CMake 4.3.0,
Python 3.14.3; the same frozen Aetheri headless compositor and Liberation Sans
font used in earlier reports. Static Release, shared Release and static Debug
ASan/UBSan/LSan each build under
`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` and pass **2/2 CTests**.
The first retains all six original worker filesystem/core groups. The second
uses real fonts and actual original-model results to check:

- Wrong ticket, view and request refusal; malformed current response retention
  and subsequent READ recovery.
- Complete ordinal identity through filtering, endpoint paging, native-style
  selection/Open actions and invalid oversized query refusal.
- German/200% 360x300 layout, including the complete 1,020-byte escaped label of
  a real 255-byte non-UTF-8 filename. Its focused row can exceed the viewport;
  top and later scrolled content retain the full semantic label and focus.

Five final independent native cases each pass **nine stages** through a separate
libatspi process and real compositor keys:

| Case | Configuration | Result |
| --- | --- | --- |
| `pknf-release` | static Release, 4,096 entries, dark | Selects last file; exact kernel device/inode and literal leaf match |
| `pknf-shared` | shared Release, 4,096, German/200%, 360x300, high contrast | Same complete workflow, with nine full compositor-output captures |
| `pknf-asan` | ASan/UBSan/LSan, same maximum/small configuration | Same workflow; normal teardown, no sanitizer diagnostics |
| `pknf-held` | sanitizer/small/maximum, real original Refresh held | Native Busy + enabled Cancel; keyboard close retires GUI before worker I/O is released and joined |
| `pknf-noime` | static Release/light, text-input protocol absent | Ordinary native editable-text filtering and Cancel work; no controlled-IME claim for this case |

The four IME-enabled cases check seven-byte preedit without committed text,
focus-loss cancellation, a later `é` commit, explicit filtering/clearing and held
ancestor navigation. Endpoint traversal reaches page 255 of the full 4,096-entry
snapshot. Native Previous/Next round trips clear selection, reveal the final row
again and reject retained old references even after returning to the same page.
Old editable-text references also refuse writes after filter/view replacement.
All five positive hosts/peers exit 0; no private child requires forced cleanup.
Process maps contain no SDL, X11, EGL or Vulkan presentation libraries.

The held-read target links a test-only wrapper around original Refresh. Production
`file_picker_native` has no gate. The retained host log orders
`ORIGINAL_REFRESH_HELD`, `GUI_GONE`, `ORIGINAL_REFRESH_RELEASED`, then
`PICKER_CLOSED picked=0 requests=6 failed=0`. This proves native GUI retirement
before that active I/O finishes, not an arbitrary-filesystem shutdown deadline.

Two isolated current-source counterexamples reject: removing copied-key checking
fails the core correlation assertion (Exit 1); keeping the same native context
fails the independent peer's first retired-row check (Exit 1). Earlier real
failures remain: an incorrect field-constructor call, a peer expecting the wrong
native role, and a page-context replacement that lost its requested focus.
The latter failed at native step 7 and was corrected by preserving focus through
replacement. No warning suppression, weakened assertion or increased timeout.

Full-output visual inspection found an awkward split of German Cancel in the
initial narrow footer; stacked actions and primary/secondary styling fix it.
The final inspected image is
`D:/NexaUI/out/picker-native-20261107-final-large.png`. This is automated-fixture
visual review, not human screenreader or approved Aetheri product-design review.

## Frozen evidence

Bundle: `D:/NexaUI/out/nexaui-picker-native-20261107.tar.gz`.
SHA-256: `7ba5537d5ccd650fdceb548f2b750f75b977672c29792d04b91931c6e1283f51`.
Size: 31,776,944 bytes; 88,989 regular files, 15 symbolic links and 1,431
directories. The copied bundle was independently verified on Alpine-Dev,
including file/directory modes, full file hashes/sizes, links, deep empty
directories and raw non-UTF-8 names. It contains 1,071 source-file provenance
records, immutable code/input archives, original full source archive, installed
SDK artifacts, builds, actual filesystem cases, JUnit, native logs/results,
captures, helpers and both counterexamples. Compiled inputs are compared to the
code archive with only text CRLF normalization; original model hashes stay raw.
Production SDK equivalence covers 159 inputs, accounting explicitly for the
historical extra root async CTest registration. Full library hashes remain those
in the worker report. `final-commands.json` and the `pknf-*-command.json` receipts
record actual build/test/native commands and outcomes.

## Remaining requirements

This advances L04/L05/L06/I01/I07/A02/A04/A06/A10/D07/P04/P05/Q02/Q04 plus
controlled T04/T11 evidence; none is closed across the catalogue by this example.
Normal Aetheri adoption, owner/modal integration and selection callbacks remain
separate. Directory/multiple/save choices, complete native error/Retry, extensive
ancestor depths/long-label native reading, continuous resize and smaller
configure fallback need further work. The checked native sizes/themes are not a
general size/platform guarantee. Wider fonts, production IMEs, real clipboard
peers, human accessibility, physical scale/outputs, performance and final design
remain open. Device/software/permission models, real inventories/operations,
trusted system hosts, durable reconciliation and later packets remain open.
