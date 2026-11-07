# Picker native input order — 2026-11-07

Consumer source: c73c2d94ddfff7a6d817c4f775c79e4440bdf139.
The installed implementation remains
ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36 (0.1.23), with original Aetheri
Files model 27de5cb228da0f37264fad64d964ad7a660d210d on its existing worker.
The [count-state](file-picker-counts-report.md) and
[recovery/resize](file-picker-recovery-report.md) reports retain their exact
earlier scope. Only the optional native picker host and its fixture change;
there is no SDK API/ABI/version/dependency or live Aetheri source/default/pin change.

## Cause and change

The host previously intercepted custom row keys while earlier Tab/pointer
events were still queued for the next context frame. A queued Tab then Home
from the last visible row therefore selected the first row using the old
focus, instead of letting Home act on the newly focused scroll thumb. Multiple
custom row keys could also all use the same old focus.

The host now completes existing interaction/focus frames before interpreting
the next application command key. It keeps at most one copied deferred event,
then uses the existing picker key handler and existing public context/event APIs.
Each handled custom key also completes its state/focus/page transition before
the following event. No second toolkit input router or private SDK access is added.
The remaining native queue is not bulk-routed across these boundaries:
nx_window_route_events transfers native events as well as assistive requests,
so it runs only after the explicit native drain has reached an empty queue.

Interaction frames may occur between commands without rasterizing each one.
The host publishes their semantics and rasterizes after the currently available
native queue drains. This is an implementation behavior, not a measured latency,
throughput, idle-power or frame-rate claim. Future platform/performance work
must measure the actual complete application path.

## Executed checks and counterexample

Fedora 44/Linux 6.19.10 x86_64, GCC 16.2.1, CMake 4.3.0 and the same installed
Wayland/software SDK. Static Release, shared Release and Debug ASan/UBSan/LSan
each pass **2/2 existing CTests**, no skips, with strict example warnings
including -Wconversion -Wshadow -Werror. Those CTests are model/view regressions;
they do not execute the native host loop.

The new independent OS peer, mixed_atspi.c, uses no NexaUI headers/linkage.
The runner stops only its test-owned host with SIGSTOP, verifies its stopped
state, sends a complete native command burst through the frozen compositor and
resumes it. It checks published key counts and semantic state after each
nonclosing burst. Six bursts per workflow cover:

- Tab/Home from the last visible row: the existing scroll thumb gains focus
  and the row selection stays unchanged.
- Shift-Tab/End back into the list: the last actual file is selected and revealed,
  crossing the explicit page boundary; old native row references refuse actions.
- A genuine pointer click in the window margin followed by Home: focus clears
  and the selected file remains unchanged.
- Home/End while the filter owns focus: its unapplied draft remains intact.
- Home/Down/End on rows: the command trace preserves first-row, next-row and
  last-row order; fresh native controls retain the draft after page replacement.
- Final Home/Down/Enter: the original worker selects the first regular file,
  and the host returns that file's exact kernel device/inode/literal-leaf witness.

Three native mixed-input workflows pass: English/static Release with 41 entries,
shared German/200%/360x300/high contrast with all 4,096 entries, and the sanitizer
equivalent. Each has five independent assistive stages plus its final closing
burst, with two backend requests total. This is finite unpaced native-batch
evidence, not an unlimited event-flood test.

Eight current-source regression workflows also pass: three seven-stage
Retry/empty-count/native-resize cases, and five nine-stage baseline cases
(Release, shared and sanitizer controlled-IME variants; sanitizer held original
Refresh/Cancel; and light/absent-IME Cancel). The held case still destroys the
GUI before releasing/joining original work. All 11 positive hosts/peers exit
normally, with no forced cleanup or accepted sanitizer diagnostic.

The unchanged previously published host from
f1da1ba0e98440c611a671b6127e0abaf240a914, byte-matched to the earlier accepted
host artifact, fails the current independent mixed-input test (Exit 1). Its first
Tab/Home burst changes selection from control ID 1,015 to ID 1,000 and focuses that old
first-row target. The new host retains selection and focuses the actual thumb.

Initial experiments remain explicit: the first draft still bulk-routed the
remaining queue and missed a key in the host counter; the peer initially expected
Open instead of the intervening thumb, then used SCROLL_BAR instead of the SDK's
actual SLIDER role. These were corrected against source and observed behavior.
An inline SSH/Python quoting attempt failed before compilation and was replaced
by a script file. Initial logs/source-input archives and peer binaries remain;
initial host binaries were rebuilt. The accepted binaries and unchanged previous
published host are retained. No deadline, warning or sanitizer suppression was added.

## Frozen evidence and remaining work

Bundle: D:/NexaUI/out/nexaui-picker-input-20261107.tar.gz.
SHA-256: a8bb6fbfcf4bb9c0ce66370841112a6b6f9b7e5f301079cd2b8403d09b9b8f07.
33,185,576 bytes; 99,975 inventoried regular files, eight links and 1,107
directories, plus the manifest. The
local copy is checked by full file hash/size/mode, directory mode and link target.
It includes code and four input archives, tested sources, installed libraries,
original model inputs/archive, build commands/JUnit, actual filesystem cases,
native traces/results and the previous host counterexample. Compiled sources
match the code archive with only text CRLF normalization. The example README was
updated from the final code archive after execution; it is not a compiled input.
159 SDK production inputs remain equivalent to the installed implementation,
accounting for the already documented extra root async CTest registration.

This advances I01/I07/A06/Q02/Q04 in the ordinary single-file picker. Wider
key layouts/modifiers/repeat, adversarial sustained input, multiple windows,
native long/deep ancestor reading, additional permission/I/O/allocation failures,
active-IME resize, other scale/output/platform combinations and human screenreader
acceptance remain open. Directory/multiple/save variants and owner/modal
integration remain the next picker compositions. Actual service/authority/durable
reconciliation, other packet-6 surfaces, packets 7/8 and performance/physical
acceptance remain open. Aetheri continues independently; 1.0.0 requires user approval.
