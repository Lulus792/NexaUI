# Native Recovery held-query lifecycle — 2026-11-07

Test/driver source: `1c8fa04942280c2189abbbe2a77dbe06f2f63c09`.
Installed SDK: `c4f467d85b0147944d2456f5e7602a39ac18e579`, 0.1.23.
Original Aetheri: `46e2a21ef979d9a76db2a8487d1de167a6a79ef7`, 67 pinned inputs.
Requirements gaining scoped evidence: D11/L11/I07/T11/A02/A06/P04/P05/Q02/Q04.

The subsequent [native keyboard step](recovery-navigation-report.md) checks
Tab/Shift-Tab, Enter refresh and filter Back/Escape/draft restoration on source
`d098be3`. Results and open gates below retain this earlier lifecycle scope.

This extends the [ordinary native observation step](recovery-native-report.md).
The SDK, original query, worker, model, view and ordinary host bodies are
unchanged. A test-only executable includes `native.c` verbatim with a renamed
entry and observes its actual public calls through GNU link wrappers. Only its
second original inventory read is held, at `SELECT o.operation_id` inside an
active SQLite transaction. The driver owns an inherited release pipe. No such
gate or wrapper is compiled into the installed SDK or ordinary host.

## Actual native ordering

The independent libatspi peer starts Read through the OS provider. While the
original transaction is held, it verifies that Read is disabled and rejects
another action, the old observation is still usable, and Close remains enabled.
The existing ordinary driver supplies the original fixtures, Wayland compositor,
private accessibility bus, input and cleanup; no alternate SQL/model is added.

In **wake** mode the peer blocks on its input checkpoint. The driver observes
its actual child main thread in Linux `poll`, reads a bounded pollfd array
through parent-owned `/proc` access, and records the waiting worker FD and a
nonzero poll timeout. It then releases SQL without native key input or another
assistive request. The real public poll returns worker-FD readability, the host
adopts observation two, and the key count stays unchanged. Only afterward may
the peer query the replacement rows, reject the retired row action and verify
Read is enabled again. It then focuses Close for ordinary native keyboard close.

In **close** mode the peer opens a filter page, writes and reads back a live
`kept draft` value, and focuses Close while SQL remains held. Native Enter must
retire the view/window and make the compositor unmap the surface while the host
process is still alive waiting for its worker. The independent peer then checks
that retained field/button references are defunct or unavailable and refuse
late edit/action requests. After that peer exits normally, the driver releases
SQL. The original query completes, the worker closes its original catalogue,
and channel teardown frees the unadopted inventory once after native retirement.
The host reports two requests and only one adopted observation.

In both modes actual original API calls remain on the worker thread. Each
process starts with five counted descriptors including the owned gate and the
counter's temporary directory FD, and ends with four after its gate and host
resources close. The extra gate accounts for the exact one-FD difference. Each
late inventory is freed once on the GUI owner thread after view/window
retirement; all host/peer processes exit normally. This tests selected native
object lifetimes, not all possible scheduling or OS failures.

## Executed matrix and counterexamples

Fedora 44, Linux 6.19.10 x86-64, GCC 16.2.1, CMake 4.3.0, Python 3.14.3.
The same installed Wayland-only/software SDKs from the prior step are used.
Consumer/test hosts build under strict C17 with conversion/shadow warnings as
errors. Each Release static, Release shared and Debug ASan/UBSan profile passes
**3/3 CTests** retaining model/worker/view regressions.

**Twelve final held-query native cases pass:** wake and close for Root v10 and
User v8 in all three profiles. Root cases use German, 32px text, 360×300 and high
contrast; User cases use ordinary size/light. Each passes three independent OS
checkpoints, original database byte/row equality, late-result/FD assertions and
normal exit without forced cleanup. ASan/UBSan and leak detection retain their
normal settings with no suppressed diagnostics. The OS peer/system libraries
are not all instrumented. Two initial Release/Root pilot passes remain separate.

The independent OS peers are compiled directly against the private real OS
headers/runtimes. Optional pkg-config discovery with a complete system
development installation is not tested here.

Two private defects build and fail their intended seven-second boundaries:

- Suppress only ticket two's worker wake: the original query finishes, but the
  paused native host does not adopt it without another event; the completion
  checkpoint times out.
- Join the held worker before GUI teardown: native unmap/GUI retirement cannot
  occur before release, so the close checkpoint times out. Failure cleanup
  releases the private gate before terminating remaining owned test processes.

The unmodified twelve-case matrix passes. Two additional **ordinary** workflows
pass through the extended driver: Release/German/narrow/high-contrast/IME and
ASan/User/light/IME, ten checkpoints each. Their ordinary host/peer paths retain
the prior behavior. No assertion or existing deadline was weakened. The initial
peer compilation's misleading-indentation error is retained; splitting its
cleanup statements fixes it with the same warning flags.

All 24 exported consumer/common/peer files bind to the exact test revision after
only CRLF normalization. Only README documentation changed after native runs;
the final source check verifies all code bytes. Physical hashes, source archives,
commands, actual poll witnesses, binaries, original fixtures and positive/negative
logs are retained in the [machine receipt](recovery-lifecycle-local.json) and
[archive receipt](recovery-lifecycle-archive.json).

The local archive `D:/NexaUI/out/nexaui-recovery-lifecycle-20261107.tar.gz`
contains 28,002,757 bytes; SHA-256
`e898f8be20dea6f239e0e5cdf7ac14c4030ea8457c0159386c8351c8f6003c55`.
Independent verification checks all 1,267 file hashes/sizes/modes, two link
targets and 483 directory modes. Sources, installed SDKs, binaries, private
original fixtures, commands and failed attempts are included; the collector's
own terminal result is outside that frozen boundary. The prior fully verified
native archive remains unchanged and is referenced by its original hash.

The prior published source `f43205b` has all seven required Windows/macOS/Linux
CI jobs successful, with two optional jobs skipped; see the
[exact-run receipt](recovery-native-ci.json). That root CI is separate from these
optional original-owner consumer/native tests. No current-source CI result is
inferred from the prior run.

## Remaining scope

Selected held-query wake, native close and whole-window stale field/action gates
now have direct evidence. Native Tab/Shift-Tab/filter Cancel, resize/compact
restore/output migration and clipboard peers remain required. The `/proc` wait
witness is specific to this parent-accessible Linux x86-64 carrier; it is not
portable OS acceptance or an idle CPU/power/performance measurement.

Production owner/service authority, reconnect/unknown outcomes, actual recovery
admission/readiness and the remaining device/software/component, installer and
trusted-surface packet 6 work remain open. Packets 7–8 and physical/human,
graphics, lifecycle and performance acceptance remain in the 103-ID scope.
No consumer pin, live Aetheri source or catalogue changed. No 1.0 release is
declared.
