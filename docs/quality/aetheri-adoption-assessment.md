# Aetheri adoption: initial source assessment

## Multi-window wait failure and lifetime probes (2026-11-05)

Source `0265f37e1287edb9e4100866edb5e26c53ba991a` adds controlled tests for
the Wayland group wait. Test-build-only hooks inject a pending-read retry and a
preparation error after the first display's real read reservation, and a poll
error after all displays have been prepared. After each case, a real Wayland
sync reply verifies that both displays remain usable. A software-buffer release
invokes a caller allocator during group dispatch: nested group polling must be
rejected, and a second variant destroys the other window and changes the
caller's window array. The outer call reports the retired window and the
survivor remains usable; the tracked allocator returns to zero live blocks.

A separate pair of controlled Wayland servers stalls a clipboard transfer on
the *second* group member; its 200 ms deadline wakes the group despite a 2 s
requested wait. Four cases passed three consecutive GCC runs and one ASan/UBSan
run (`/tmp/nexaui-many-proof-final/result.json`,
`/tmp/nexaui-many-proof-asan/result.json`); the same four passed with a shared
SDK (`/tmp/nexaui-many-fault-shared-run/result.json`). Five neighboring native
wait, AT-SPI and clipboard cases passed (`/tmp/nexaui-many-fault-neighbors/result.json`);
the dual-backend build passed the transfer and X11-unsupported checks. A
tests-disabled shared build did not export the test fault hooks.

In isolated negative copies, removing read cancellation timed out for each
of the three injected paths after a forced real sync, ignoring the second
window's deadline failed the bounded-time check, and using the caller's mutated
array instead of the retained snapshot caused a segmentation fault. An earlier
version of the cancellation test did not detect the second fault without the
forced sync; that weak result is not counted. The first shared-build run used
an already existing output directory and was rerun in a fresh one. These are
selected fault-injection and private-compositor checks, not exhaustive
reentrancy, all clipboard/drop interleavings, physical input or Aetheri
acceptance.

## Wayland multi-window wait (2026-11-05)

Source `c254f0a0bbf826dfdbc94412311d7bb1e1586ffb` adds Linux
`nx_window_poll_many` for up to 16 distinct owner-thread Wayland windows and
one optional borrowed application fd. The implementation retains the window
wrappers, includes accessibility and transfer descriptors/deadlines, rejects
nested polling, and releases every prepared Wayland read before dispatching
callbacks. X11 windows return `NX_ERROR_UNSUPPORTED`. The development package
version is 0.1.3; this is not a release or a 1.0.0 approval.

On private Weston/Xvfb, a real virtual keyboard switched between two windows;
the group wait woke for each and for the application pipe, then the surviving
window rendered and received input after close. Separate checks covered idle
CPU, timeout, wrong thread, invalid and duplicate inputs, fd hangup/error,
16 windows and a disconnected peer. The final FD/capacity check passed with
GCC and ASan/UBSan (`/tmp/nexaui-many-fd-final/result.json`,
`/tmp/nexaui-many-fd-asan/result.json`). Five group-wait, AT-SPI and clipboard
cases passed with each compiler (`/tmp/nexaui-many-last-native/result.json`,
`/tmp/nexaui-many-last-asan/result.json`). The AT-SPI case used an independent
client and included a second window and reconnect; the clipboard cases used a
single-window group. Headless Sway targeted-close passed
(`/tmp/nexaui-many-sway-final/client.log`). Seventeen selected native and
protocol regression cases, four simple-IME cases and the X11 unsupported/legacy
checks passed. Separate strict-C17 consumers built and ran against static and
shared 0.1.3 installations; configuration rejected 0.1.2.

The exact partial-preparation/reentrant-destruction interleavings and transfer
deadlines on a different group member still need targeted fault-injection;
these tests do not establish complete W02/W10 acceptance, real Aetheri behavior,
physical input or human screenreader acceptance. An initial dual-backend build
used a nonexistent target name and was rerun with the correct target.

## Preparing a shared Wayland wait (2026-11-05)

Source `b167379c14cf11348d3c788c93c0cd66add23b92` separates the existing
single-window Wayland poll into preparation, descriptor/deadline collection,
and completion. The public API and single-window behavior are unchanged. A
successful preparation reserves a Wayland read and must be finished even after
timeout, interruption or error. This is groundwork, not a multi-window wait.
A future composition still needs owner/lifetime retention, cancellation of all
prepared reads on partial failure, already queued work, accessibility hub FDs
and deadlines, application FDs, per-window failures and bounded storage.

Forty-six GCC protocol, FD, clipboard and drop tests and 21 Clang ASan/UBSan
cases passed. On private Weston/Xvfb, two-window input, independent drop and
Settings AT-SPI reopen passed (`/tmp/nexaui-pollsplit-native/result.json`), as
did all four simple-IME cases (`/tmp/nexaui-pollsplit-simple-ime/result.json`).
The headless Sway targeted-close test passed (`/tmp/nexaui-pollsplit-sway/client.log`).
A dual-backend build passed four selected Wayland tests and the X11 window test
under Xvfb. An initial dual-build test command named a nonexistent target; a
corrected command ran the actual tests. No physical input, Aetheri compositor,
or efficient shared idle/wakeup behavior is established by this refactor.

## Compositor-requested close with two Wayland windows (2026-11-05)

Source `1fce23a381cee879763cb2fceb562dad66113b26` extends the two-window
host to react to `nx_window_info.should_close` and verify that the other
window has no close request. An isolated, unprivileged headless Sway 1.12-rc3
session targets window A through Sway IPC; the host observes and handles that
close, while B remains in Sway's tree, submits another software frame and is
reported focused after a focus request. Two independent GCC runs and one Clang
ASan/UBSan run passed (`/tmp/nexaui-w02-sway-final1`, `-final2`, `-asan`). A
fresh software-only build also ran the registered optional CTest successfully
(`/tmp/nexaui-w02-sway-ctest-final/client.log`); the existing virtual-keyboard
Weston test passed after the host change (`/tmp/nexaui-w02-sway-westonguard/result.json`).
The Sway harness requires an explicitly provided compositor and refuses to use
an existing session. Before using Sway, an isolated Weston attempt with
`Super+K` ended the host without a close acknowledgement; this was not counted
as a passing native close test. See [Weston's bindings manual](https://man.archlinux.org/man/weston-bindings.7.en)
for that environment's kill binding. Headless IPC focus is not physical input,
a displayed-pixel check, real screenreader acceptance, Aetheri compositor
coverage, or an efficient multi-window event loop.

## Independent Wayland windows (2026-11-05)

The earlier W02 source pointer `tests/test_multi_window.c` is Windows-only.
Source `453f42894226d31c8baf9e8ba2bd43a0e78154c9` adds a public-API Linux
host and a private Weston/Xvfb driver. Two separate windows, contexts and
software views submit frames; virtual keyboard input is routed to the focused
window, Weston switches focus, and the application closes one window. The
survivor submits another frame and receives distinct keyboard input. The driver
checks the complete text-event sequence for duplication or misrouting. The
final GCC test passed three consecutive runs (`/tmp/nexaui-w02-final/result.json`)
and passed once with Clang ASan/UBSan (`/tmp/nexaui-w02-asan/result.json`).
An initial fixture run did not observe a close acknowledgement; replacing the
signal file atomically removed a possible partial-write race, but its cause was
not independently isolated. The next run sent the survivor's key before
focus was observed; waiting for the compositor's actual focus event corrected
that ordering. The test polls each connection with a 5 ms bound and redraws
continuously; it does not establish an efficient idle event loop. Submitted
frames are not proof of visible pixels. Compositor-requested close, Aetheri's
compositor, multiple outputs, physical input, human accessibility and Vulkan
multi-window behavior remain open.

## IME reply after field replacement (2026-11-05)

On parent `febf5ad8981f928573814a02d26f5186f7b290fa`, a controlled Wayland
v3 test queued an IME delete and commit, destroyed the original text field and
constructed an identical replacement. When the allocator actually reused the
field address (`NEXAUI_TEST_REQUIRE_FIELD_REUSE=1`), the old reply edited the
replacement and the test failed. Source `90aac97fe157500a13f7765540ab6e6ffbe08c54`
includes the already assigned text-model identity in the IME revision; the
same forced-reuse test passed. The normal test also runs without requiring a
particular allocator to reuse an address. Ten controlled v1/v3, text-value and
Settings protocol cases and five Clang ASan/UBSan cases passed; the sanitizer
case does not claim address reuse. Three selection/IME clipboard cases passed.
On isolated Weston/Xvfb, all four `weston-simple-im` cases passed
(`/tmp/nexaui-ime-reuse-simple-final/result.json`), as did the native Settings
keyboard and AT-SPI cases (`/tmp/nexaui-ime-reuse-atspi-final/result.json`).
Before the fix the four simple-IME and 15 selected clipboard cases also passed;
the first attempted simple-IME invocation failed because `setxkbmap` was not
on PATH, then passed using the private binary. These are Linux fixture results;
other IMEs, physical input, other platforms and human accessibility remain open.

## Settings text selection across view replacement (2026-11-05)

Source `75958824484c4e373ec5ec49815fde357dfd0cf8` adds
`nx_text_field_set_selection` using grapheme-boundary UTF-8 byte offsets and
increments the development package version to 0.1.2. Changed selection
invalidates pending asynchronous edits; an unchanged selection leaves them
alone. IME revisions also incorporate edit serials so changing a selection and
returning it to its old offsets cannot admit an old IME response. The Settings
session saves and restores anchor and cursor; undo history and active
composition are not transferred.

The scene test replaced a reversed selection containing a multibyte character
after reopening. API tests covered combining/emoji boundaries, invalid offsets,
unchanged selection, undo/redo and stale queued semantic edits. A controlled
Wayland v3 fixture rejected an IME response after selection changed and returned
to the same offsets. Twenty-nine text, input and Settings tests passed on private
Weston/Xvfb (`/tmp/nexaui-field-selection-regression/result.json`); four focused
cases passed under Clang ASan/UBSan. The independent AT-SPI client observed the
restored selection. Separate Settings consumers built and passed that client
against freshly installed static and shared 0.1.2 SDKs
(`/tmp/nexaui-field-012-native-OFF/result.json` and
`/tmp/nexaui-field-012-native-ON/result.json`); configuration rejected the older
0.1.1 installation. These are Linux virtual-input checks, not Windows/macOS,
physical input, human accessibility acceptance, real service reconciliation,
or an ABI compatibility guarantee. No release tag or 1.0.0 approval is implied.

## Settings focus across view replacement (2026-11-05)

Example source `4d793b52d9f6d0d0fc6b3d23f3b02d7b3edcf1b3` transfers the
example's logical control focus and scroll offset alongside its application
session. The first rebuilt frame restores an available control unless new
input is already queued. A disabled Apply falls back to the checkbox; the
following frame reveals the focused control if needed. The model test checks
the disabled and enabled cases, a 128x150 viewport, new-input precedence and
a genuinely changed scroll position, with GCC and Clang ASan/UBSan. The
independent AT-SPI client focused the checkbox, replaced the view and observed
the new accessible as focused. Twenty scene, protocol, keyboard and native
AT-SPI cases passed on private Weston/Xvfb
(`/tmp/nexaui-settings-focus-regression/result.json`); the additional final
model cases were rerun separately. This uses virtual input and a simulated
service. Text selection, undo history, composition, human screenreader review
and real service reconciliation remain open.

## Settings view replacement (2026-11-05)

Example source `d612e51a04551e98f21aac707c0a1b7d2849445f` transfers its
draft and operation state into application-owned memory when a view is closed.
The host retires the old native accessibility bridge before destroying its
context. An already claimed operation remains pending without being claimed
again; the owner must query the original backend and report the result. The
model test covers an unknown result, stale completions, a later draft, and an
authoritative completion while no view exists. It passed with GCC and Clang
ASan/UBSan. Twenty scene, protocol, native keyboard and AT-SPI tests passed on
private Weston (`/tmp/nexaui-settings-session-regression/result.json`). A copied
example also passed the independent AT-SPI reopen test against static and
shared installed SDKs from `4e6c240bda94d8bc537851cdbb89846003eb86e3`
(`/tmp/nexaui-session-d612-external-ON/result.json` and
`/tmp/nexaui-session-d612-external-OFF/result.json`). These use a simulated
service and virtual input. Real Aetheri service reconciliation, durable or
cross-process recovery, restoration of focus, scroll and undo state, and human
accessibility acceptance remain open.

## Settings uncertain operation outcome (2026-11-05)

Example source `619d8c387e34807cd66f4bd70ef167d871416005` distinguishes an
unknown Apply outcome from confirmed success or failure. The app-owned token
remains pending, the status is visible and semantic in English/German, Apply is
disabled and the host can claim a submission only once. A later authoritative
completion of the same token updates only the submitted snapshot, preserving a
newer draft; a confirmed failure permits a new request. Stale tokens are
rejected. `--simulate-unknown` demonstrates an unresolved outcome without
pretending that the example backend provides a service or recheck operation.

The scene test exercised dispatch exactly once, unresolved and stale tokens,
draft changes, language changes, invalid drafts and subsequent success/failure;
it passed under GCC and Clang ASan/UBSan. An independent libatspi client saw
the unknown status, rejected another Apply and verified the later draft was
preserved without automatic completion. Nineteen Settings scene, v3 protocol,
native keyboard and AT-SPI tests passed on private Weston
(`/tmp/nexaui-settings-unknown-final/result.json`). A copied example from this
source also passed the independent client against static and shared installed
SDKs from `4e6c240bda94d8bc537851cdbb89846003eb86e3`
(`/tmp/nexaui-unknown-619-external-ON/result.json` and
`/tmp/nexaui-unknown-619-external-OFF/result.json`). This is virtual-input and
simulated-service evidence, not real backend reconciliation, reopening after
scene destruction, human screenreader acceptance or all L11 states.

## Wayland selection offered before focus (2026-11-05)

The [Wayland core protocol](https://wayland.freedesktop.org/docs/html/apa.html)
allows selection delivery immediately before keyboard focus and limits an
external offer's focus lifetime. A controlled test on parent
`1c61a6e93478086985d0709a7fb648a9ee79c22b` demonstrated that an external
offer received after focus loss but before `keyboard.enter` could nevertheless
be read. Source `6eac7ef25d79cc4d2544710324b53d598f73906d` rejects external
reads without native keyboard focus in the async, synchronous and internal
fetch paths. A fresh offer is retained through enter, while re-entry without a
fresh offer cannot revive the old one. App-owned clipboard source handling is
unchanged. The test server now emits real selection-before-enter ordering.

Forty-three controlled clipboard/drop/Settings cases, eleven native Weston
grid/AT-SPI cases, six Clang ASan/UBSan cases, default dual-backend X11 and
Wayland checks, and four separate simple-IME cases passed. The first broader
224-case run had 217 passes, four opt-in IME skips and three failures: those
three independent AT-SPI test hosts had not simulated native keyboard focus.
After updating that fixture, all three passed on rerun; the full 224-case run
was not repeated. The four skipped IME cases were run separately and passed
(`/tmp/nexaui-focus-guard-ime/result.json`). Initial and final fixture logs are
`/tmp/nexaui-focus-guard-full.log`, `/tmp/nexaui-focus-guard-second.log`,
and `/tmp/nexaui-focus-guard-native/result.json`.

This protects against reading before focus and against reusing stale data; it
does not make Weston re-send a missing offer after the redundant focus cycle
recorded below. That scenario remains unavailable rather than being silently
recovered with potentially obsolete clipboard content. Human and other
compositor acceptance remain open.

## App-owned Wayland paste demonstration (2026-11-05)

Example source `07bbbcae31358dcd5f47a3db817c99329b812349` uses the public
asynchronous Wayland read for Ctrl+Shift+V. It copies at most 4096 bytes into
an application-owned in-memory buffer, reports status visually and in a
semantic status node, and never sends bytes to a PTY or interprets them. A
pending request is cancelled on focus loss, relevant model/selection changes or
close; application-owned focus and revision are checked before committing a
result. Clipboard data is cleared from this demonstration buffer at shutdown.

Private Weston/Xvfb and independent `wl-copy`/`wl-paste` peers exercised
multiline UTF-8, a combining mark and escape bytes; a test-only checksum checked
the received bytes without logging them. The clipboard owner was paused while
the app changed its model or lost focus; both pending reads were cancelled and
no late completion was observed during the follow-up waits. The paste case passed three consecutive runs
(`/tmp/nexaui-cell-paste-repeat/result.json`) and all five existing/new native
grid tests passed (`/tmp/nexaui-cell-paste-regressions/result.json`). An
independently copied example from this source passed the same paste and
cancellation test against static and shared installed SDKs from
`4e6c240bda94d8bc537851cdbb89846003eb86e3`
(`/tmp/nexaui-cell-paste-07b-external-OFF/result.json`,
`/tmp/nexaui-cell-paste-07b-external-ON/result.json`).

During test setup, an additional forced X11 focus cycle after a `wl-paste`
peer closed produced a Weston keyboard leave/enter without a renewed selection
offer; the adapter discarded the previous offer and the next paste reported
unavailable (`/tmp/nexaui-cell-paste-debug.log`). The successful fixture avoids
that redundant focus cycle; it does not resolve the observed recovery case.
This demonstration is not a terminal emulator, PTY integration, full paste
policy, installed-IME acceptance or human visual/screenreader acceptance.

## Application-owned asynchronous Wayland clipboard read (2026-11-05)

Source `4e6c240bda94d8bc537851cdbb89846003eb86e3` exposes the existing
bounded, poll-driven native clipboard read in `<nexaui/wayland_clipboard.h>`.
A caller owns a token and the returned bytes; focus/model/selection validation
and paste policy remain application responsibilities. Text-field paste cannot
cancel a pending application-owned read; the window still has one shared read
slot. The API rejects unsupported X11 windows and does not change the portable
synchronous getter.

GCC Wayland protocol tests covered a stalled source, continued IME work,
completion, timeout, over-limit and malformed content, cancellation, focus and
seat loss, stale tokens, wrong-thread calls and the shared-slot guard. Ten
clipboard/focus tests passed; 22 Settings/AT-SPI paste regression tests also
passed. Clang 22 ASan/UBSan passed the asynchronous and focus/seat cases. A
dual-backend shared build passed the X11 unsupported check under private Xvfb
and the Wayland async case. From an archive of this exact source, separate
static and shared Wayland-only installed SDKs and copied C17 package consumers
built and ran, including the new header and null-argument calls. These package
checks do not themselves perform a live clipboard read; the protocol fixture
is a separate controlled server, not a human desktop or Aetheri integration.
No terminal paste model, cross-platform asynchronous API or human acceptance
is claimed by this step.

## Application-owned cell selection copy (2026-11-05)

Example source `c7ceb2738d5a2cf3bfc0febcb51b1699ca827281` copies its
application-owned visible selection with Ctrl+Shift+C through the existing
Wayland clipboard API. It converts its cell endpoints to the corresponding
UTF-8 snapshot range; empty selection leaves the current clipboard intact.
Ctrl+C is not consumed as a copy shortcut by this example. A copy failure is
reported on standard error. Clipboard persistence after owner exit is a
compositor policy, not a guarantee made by the example.

An independent `wl-paste` 2.2.1 peer and AT-SPI client on private Weston/Xvfb
verified copied wide text, a combining mark, a line break, reverse selection,
empty selection and Ctrl+C preservation. The case passed three successive
runs (`/tmp/nexaui-cell-copy-final/result.json`); the pre-existing grid native
keyboard, selection and AT-SPI tests also passed. A copied example from this
source passed the same peer test against static and shared installed SDKs from
`a91b30377209f57854fcfc44c171d792c1f22195`
(`/tmp/nexaui-copy-c7-consumer-OFF/result.json` and
`/tmp/nexaui-copy-c7-consumer-ON/result.json`). These are virtual-input tests,
not human acceptance or a terminal implementation. Paste policy, PTY input,
scrollback, clipboard-manager persistence and accessible mutation remain open.

## Application-owned cell selection (2026-11-05)

Example source `07cb954062d18f597f3dd01538ab594d710caf37` keeps a cell
cursor and anchor in its own model. Left/Right and Shift movement skip a wide
continuation, paint the selected visible cells and independently map the
selection to UTF-8 byte positions in the read-only semantic snapshot. The
AT-SPI provider exposes scalar positions; native selection mutation remains
rejected. No terminal parser, clipboard or PTY ownership was added.

Private Weston/Xvfb tests with virtual keyboard and an independent libatspi
client checked forward and reverse selection around the wide cell, a combining
mark, and a line break; they also checked selection mutation rejection. The new
native selection case passed three consecutive runs
(`/tmp/nexaui-cell-selection-final/result.json`). Original native keyboard,
read-only AT-SPI and semantic text tests passed as well. A separately copied
example from this exact source was built against static and shared installed
SDKs from `a91b30377209f57854fcfc44c171d792c1f22195`, and both passed the
same selection client on private Weston
(`/tmp/nexaui-selection-07cb-consumer-ON/result.json` and
`/tmp/nexaui-selection-07cb-consumer-OFF/result.json`). This is not human
screenreader or visual acceptance, production terminal selection policy,
scrollback, full IME integration or an accessible mutation interface.

## Wayland build in an X11-free test root (2026-11-05)

Git archive source `009fc6b2f652d36084fb23aa45de4e81083f82c5` built and
installed as static and shared C17 Wayland-only SDKs in a separate Alpine
3.24.2 filesystem. Independent package consumers compiled and ran inside that
filesystem. The sandbox mounted the root and source read-only, used a private
temporary filesystem and an isolated network namespace, and checked standard
X11/XCB header, library and pkg-config locations before building. Both package
checks rejected a required Vulkan component as expected. D-Bus development
headers were present and both builds compiled the AT-SPI implementation; this
is not a native AT-SPI or IME exercise.

The root contained GCC 15.2.0-r5, CMake 4.2.3-r0, Wayland 1.25.0-r0,
wayland-protocols 1.48-r0, libxkbcommon 1.13.1-r0 and dbus-dev 1.16.2-r2.
Alpine's `libxkbcommon-dev` package requires X11 libraries even though this
consumer does not: its signed package was verified, then only its non-X11
headers, core linker symlink and `xkbcommon.pc` were extracted into the test
root. Its SHA-256 was `94fd46087a1830db5238a37cf913ebef8c831ea682d53d0a498b26637f5caa7c`.
The root still contains XKB keyboard data, which is not an X11 library. The
source archive SHA-256 was `a5689006216eae19810b5f9899bf73f7cfaf8d7069ddef92056945b49dfc7fd2`.

The reusable [root checker](../../tests/check_wayland_no_x11_root.py) also
rejected a deliberate X11-header counterexample. Package listing and successful
log are `/tmp/nexaui-no-x11-root-009-packages.txt` (SHA-256
`b787780caba9c0c8c7e8af55e0673509fc25021d6c65fff220dae8271443f153`) and
`/tmp/nexaui-no-x11-009.log` (SHA-256
`e0189af7991b29c34f6b01fbda6b2cd1d115b27f20902f9081e37fc2d2b1479e`).
The package probe uses synthetic source-revision declarations to test metadata
propagation; the archive identity above is a separate source observation. This
does not replace default dual-backend, native-compositor, installed IME,
screenreader or other-distribution acceptance.

## Minimal offline software profile (2026-11-05)

Static and shared Wayland-only SDKs built from Git archive source
`d97c9495b50963cf25c6d06a1994a5240260e1fc` with Vulkan, X11 and AT-SPI
disabled. Separately copied Settings consumers built against each installation,
identified the linked source revision and disabled AT-SPI bit, used software
rendering and passed the private Weston/Xvfb virtual-keyboard workflow and
normal close with inaccessible D-Bus addresses. `ldd` checks excluded Vulkan
and linked D-Bus/AT-SPI/GLib; the shared core resolved from the selected prefix.
An AT-SPI-enabled SDK was rejected by the disabled-profile probe (exit 24).
Final fixture results: `/tmp/nexaui-offline-d97-final-ON/result.json` and
`/tmp/nexaui-offline-d97-final-OFF/result.json`. The existing enabled-AT-SPI
path was rerun against shared source `a91b30377209f57854fcfc44c171d792c1f22195`;
its keyboard and independent AT-SPI Apply/retry checks passed
(`/tmp/nexaui-enabled-a91-regression/result.json`).

This does not prove an X11-free filesystem, installed-IME operation, human
accessibility, or an actual installer/recovery workflow. The disabled profile
has no screenreader bridge. The [dependency profile](../concepts/dependencies.md#minimal-wayland-software-build-for-offline-hosts)
states host, font, operation ownership and fallback requirements.

## Application-owned accessible text snapshot (2026-11-05)

Source `d11786f68358d75488534623f729a94a298251c0` adds an explicit,
copied, bounded read-only UTF-8 semantic snapshot. The application supplies
the lifetime identity, visible text and byte positions. Linux exposes the
supported AT-SPI Text reads in Unicode scalar offsets and rejects native
caret/selection mutations. The cell-grid example independently constructs its
visible text, including a wide cell and combining mark, from its own model.

On Alpine WSL, GCC and a Wayland-only build succeeded. The independent libatspi
client passed on private Weston/Xvfb with a private registry: it read the text,
checked scalar count and caret, and verified rejected caret/selection requests.
The native keyboard, semantic snapshot and existing Settings AT-SPI regression
also passed. A 218-test private-compositor run reported one failure and four
skips: `nexaui_settings_wayland_idle` saw zero long waits. The same test failed
with the same observation in an independently archived and rebuilt parent
`c876ee700f368668cce87f389ce76d90849d87d8` under the same fixture. The four
simple-IME cases were skipped; Weston/Xvfb cleanup also reported an error in
the full run. Logs: `/tmp/nexaui-cell-snapshot-full.log`,
`/tmp/nexaui-cell-snapshot-parent.log` and
`/tmp/nexaui-cell-snapshot-recheck/result.json`.

This is not terminal, PTY, scrollback, IME, editable text, selection control,
full document navigation, another platform's Text interface or human Orca
acceptance.

The subsequent source `a91b30377209f57854fcfc44c171d792c1f22195` was
exported with `git archive` and built as separate static and shared Release
Wayland-only SDKs, with Vulkan disabled. The package consumer checked the linked
source revision and Wayland-only features for each. A copy of the grid example,
configured independently against each installed prefix, passed virtual native
keyboard and independent AT-SPI text/read-only checks on private Weston. `ldd`
resolved the shared NexaUI library from the selected prefix; the static example
did not load a shared NexaUI library. These checks cover this example and the
tested Alpine/WSL environment, not all downstream applications.

The earlier idle-test failure was reproduced with no accessibility bus: Settings
intentionally retries the unavailable bridge every second. With a private
AT-SPI registry the original long-wait observation passed. Separate tests now
cover the available-bus long wait and bounded unavailable-bus retry; they do
not turn a missing accessibility service into a successful accessibility test.
After that test correction, the Wayland-only private-compositor matrix reported
215 passed, zero failed and four skipped simple-IME tests out of 219. The
fixture reported `passed`; its log is `/tmp/nexaui-idle-fix-full.log` and its
result is `/tmp/nexaui-idle-fix-full/result.json`. This run used virtual input,
not human acceptance.

The four opt-in simple-IME tests were subsequently run against build source
`279a5c9e69e7e05961ed3dfb239fef760f9da969` on a separate owned Xvfb and
Weston 14.0.2 session with `weston-simple-im`, a private `compose:ralt` keymap
and a D-Bus session. All four executed and passed: composition, window focus
loss, focus transfer to another control, and selection replacement. The helper
`tests/run_simple_ime_session.py` checks the JUnit records and fails if a case
is missing, skipped or failed; the final repeat's report and compositor metadata
are in `/tmp/nexaui-279-ime-fixture-final/`. Initial fixture attempts exposed
that TCP Xvfb does not provide the needed shared-memory output and that Weston
also requires `XKB_DEFAULT_OPTIONS=compose:ralt`; neither was counted as a pass.
The test uses virtual input and does not establish human or physical acceptance.

## Fixed-cell drawing candidate (2026-11-05)

Candidate based on `88aeef97f0cd9b90f335e8520cae65570994ba09` adds
`nx_draw_cell_grid` and logical hit testing for an application-supplied visible
cell snapshot. The caller chooses font/glyph IDs, cell spans, combining-glyph
positions and selection colors; there is no implicit terminal parser, text
shaping or accessible text implementation. Limits cover visible cells, fonts,
glyph-cache budgets and copied A8 bytes. Font and source cells are borrowed
only during drawing, with rollback on failure.

GCC static Wayland-only and dual-backend tests and Clang shared ASan/UBSan
tests passed. Controlled cases include a 240×100 visible grid, wide-cell hit
testing, positioned multiple glyphs, clipped rendering, all observed allocator
failure positions, mask budget, font-count limit and use after the source font
is closed. Seven text/raster/cache regression cases passed in both GCC and
Clang configurations. A separate application-owned model accepted native text
and cursor keys, skipped a wide continuation and closed on private Weston;
the same virtual-keyboard case passed with ASan/UBSan. It does not establish
actual terminal/PTY behavior, retained glyph-atlas performance, Aetheri
integration, IME, accessible text or human/physical acceptance.

For committed source `8e6d1300ea3de9de06c27ee94c5747859e2d0821`, static and
shared Wayland-only SDKs were built from a Git archive with Vulkan disabled.
Separate copied C17 grid examples and package probes built against each
installation without CMake X11 discovery; probes checked the declared linked
core revision and called both grid APIs. Each example received native virtual
text and right-arrow input on private Weston, advanced over a wide-cell
continuation in its own model and closed cleanly. Both fixture result records
show child, compositor and overall exit 0. Compile commands use copied source
and installed headers; `ldd` resolves the shared SDK from its chosen prefix and
the static consumer has no shared NexaUI dependency. The example uses DejaVu
Sans Mono from the test environment; font coverage, especially the wide sample,
is not asserted and missing characters may use `.notdef`. Local artifacts:

- `/tmp/nexaui-cell-8e6-consumer-ON.log`: SHA-256 `c70ea02cf12cbd13586ef8916a6c69ff46c12df5eb3a9965e9dd23be2905392c`
- `/tmp/nexaui-cell-8e6-consumer-OFF.log`: SHA-256 `03b931cbb80943ade0c062edad1a7e907e831f407937a62d4bbfbb0ea3da3d2c`
- `/tmp/nexaui-cell-8e6-native-shared/result.json`: SHA-256 `9438f1a9adecde10f9d873d8f3a06b7d5159a4a78b02a1be005ae3b856e41518`
- `/tmp/nexaui-cell-8e6-native-static/result.json`: SHA-256 `e2876ad311877699210a65ef283e1d4574272bb97cf4f1912bfda582722a0a99`

## Host-owned Wayland worker wake candidate (2026-11-05)

Candidate based on `dac7e21b8b9422bf2103a8db4ad3549ca0b78a5f` adds
`nx_wayland_shm_host_poll`: the owner thread dispatches its selected borrowed
Wayland queue and waits for that connection plus one application-owned readable
descriptor. The host owns the descriptor, message and deadline policy. A
controlled fake-role test checks a real delayed worker wake, readable plus
hangup, invalid descriptors, prequeued events without a long sleep, both default
and dedicated queues, wrong-thread and callback-reentry rejection, and display
loss. GCC Wayland-only and dual-backend plus Clang shared ASan/UBSan passed.
The installed-SDK host example used private Weston to configure a host-owned
ordinary role, submit a frame and receive a worker reply. This does not provide
an AT-SPI bridge for host-owned surfaces, general service persistence or proof
of actual Aetheri private-role, idle-performance or physical acceptance.

The committed source `3cbfe22b3bfe160adfa33f1efec15e7a951fe147` was exported
with `git archive` and built as static and shared Wayland-only SDKs with Vulkan
disabled. Separate copied host consumers and package probes compiled against
each installation with X11 discovery disabled; the probes compared the linked
core's declared revision to its package. On private Weston, both consumers
configured an ordinary host-owned role, queued a frame, received the worker
reply and exited with the compositor and child both at exit 0. The shared
consumer resolved NexaUI from the selected prefix; the static consumer did not
load a shared NexaUI library. Compile commands use the copied source and
installed headers. Local artifacts and SHA-256:

- `/tmp/nexaui-host-poll-3cb-consumer-ON.log`: `b2f71a69b22c06e134cb997cd940499ad0a4da287e4ab751ee41c2c1e6021867`
- `/tmp/nexaui-host-poll-3cb-consumer-OFF.log`: `7e25bb5cd0b7834cf1d17e9563915278292928e2ff35627f1c08e9a1eace3e81`
- `/tmp/nexaui-host-poll-3cb-native-shared/result.json`: `7fcad21955c3803aa3cda449d48db0de58884aac83dbf5240888c1333af96750`
- `/tmp/nexaui-host-poll-3cb-native-static/result.json`: `d4ea2ab58bedbe7541c8fb99d8f7587df34cf639c0d21f5fa1d7ac652c3a7a9e`

The current isolated Alpine environment lacks the pinned wlroots development
dependencies of Aetheri's private compositor. No existing Aetheri development
session was started or changed, and its separate SDL consumer remains usable.

## Host-owned Wayland surface candidate (2026-11-05)

Candidate based on `48123ef1bfdba28986c7f86790005fa5ddf40c5d` adds
`nx_wayland_shm_host_*`: the host supplies its connection-bound `wl_shm`,
already role-assigned surface and event queue. NexaUI does not open a second
connection or assign a role; the host owns configure, scale, input, semantic
publication, dispatch and flushing. A controlled server with a fake private
role checks host configure/ack before the first attachment, transparent and
premultiplied pixels, separate queue dispatch, bounded buffers, wrong-thread
errors and destruction with an outstanding submission. GCC Wayland-only and
dual-backend tests and Clang shared ASan/UBSan tests passed. A standalone C17
consumer built against an installed Vulkan-free SDK, owned an ordinary xdg role,
and queued a frame in private Weston. These tests do not prove Aetheri private
role admission, actual dock/search behavior, input/AT-SPI integration, physical
presentation, three-output operation or compositor-loss recovery.

The controlled host test also checks that a known connection loss returns a
platform error and that outstanding adapter-owned buffers can be destroyed;
it does not recreate the lost connection or role. An initial version of this
test destroyed a server-side client inside its active request callback and
crashed the test server; the corrected fixture shuts down the socket and lets
the server retire the client normally. The corrected GCC and Clang ASan/UBSan
tests pass.

For committed source `52703944c85ae9c79fd4df00fb1a70f69fbc10fd`, static and
shared Wayland-only SDKs were built from `git archive` with Vulkan disabled.
Separate copied C17 host examples and package probes built against each
installation with X11 discovery disabled; the probes checked the declared
linked-core revision. The example's compile commands contained only the copied
source, generated protocol bindings and installed SDK headers. Both examples
configured their own ordinary xdg role and queued a frame on private Weston;
both fixtures report child, compositor and overall exit 0. `ldd` resolved the
shared core from its tested prefix and showed no Vulkan dependency; the static
consumer had no shared NexaUI dependency. Local artifacts and SHA-256:

- `/tmp/nexaui-host-527-consumer-ON.log`: `ddf78136043076ce0f85ed9f00dc5b891ba7b2d6337b8f64755e9bcbf76f1c80`
- `/tmp/nexaui-host-527-consumer-OFF.log`: `93c4ad7ee93e685c0934749074860fd2b8b786f2999cfc38d2fe7e6816de0658`
- `/tmp/nexaui-host-527-native-shared/result.json`: `3c95253814bf2b566fa3765fe4eb78371ccb5487ad50b536f40a5826ca4d846a`
- `/tmp/nexaui-host-527-native-static/result.json`: `9f1828a02014dd771992de22c63e9d292fdd947af5b46f9ea3ffec656d1092c9`

The Aetheri private shell, its three outputs, input, semantics and real
compositor integration were not tested or modified.

## Settings renderer selection candidate (2026-11-05)

Candidate based on `66fdd9aaa7729221db54c616fceb3aacae9db2ac` uses the
existing public views: Settings attempts Vulkan first when available and falls
back to software only for `NX_ERROR_UNSUPPORTED`, including during rendering
after Vulkan has safely closed. Other errors are preserved. A Vulkan-linked
binary still requires its loader at process startup; an SDK without Vulkan or
an explicit software build does not. Selection belongs to this example, not
an automatic toolkit-wide policy.

On Alpine-Dev/WSL2, fake-backend tests checked selection, memory errors,
unsupported rendering, failure to retire, software-open failure and device
loss; GCC and Clang ASan/UBSan passed. Private Weston/Xvfb with Vulkan ICDs
disabled ran the same Vulkan-linked Settings binary through software fallback,
keyboard, AT-SPI failure/retry and simple-IME composition. The keyboard case
also passed with Clang ASan/UBSan. A normal Vulkan virtual-keyboard run passed
separately. An installed SDK without the Vulkan component built the copied
consumer with Vulkan discovery disabled and passed keyboard and AT-SPI checks.
These are virtual-input tests, not physical, Aetheri-compositor or human
acceptance; production input methods and performance remain open.

For committed source `0924790e3d21b76fd91014dd10e5a2973dfb7707`, separate
SDKs were built from a Git archive: shared Wayland-only with Vulkan and static
Wayland-only without Vulkan. A copied Settings consumer was built and installed
against each. With the Vulkan SDK and both Vulkan ICD environment variables
pointing to a nonexistent manifest, the consumer selected software and passed
keyboard and independent AT-SPI failure/retry. With the Vulkan-free SDK and
Vulkan discovery disabled, the consumer built and selected software without a
Vulkan runtime dependency and passed the same checks. Package metadata and the
linked core declared the tested revision; both fixtures recorded child,
compositor and overall exit 0. Local logs and SHA-256:

- `/tmp/nexaui-auto-092-installed-vulkan.log`: `5b0743b62ff62661bf8d1dd0a8f47fd80205d07bebcbe42e0e0bebf6f8dc4cc9`
- `/tmp/nexaui-auto-092-installed-software.log`: `8966cec5dbdde99b402e13a0a04400b2c5351c5287f6728a115a442cf54a5872`

## Wayland software Settings candidate (2026-11-05)

Candidate based on `94c3074ea50a247a04270d32170965c7cb40d7ec` adds the
Wayland `nx_software_view_*` API and a selectable Settings software runner.
On Alpine-Dev/WSL2, GCC Wayland-only with Vulkan disabled passed controlled
shared-memory tests for premultiplied pixels, the three-buffer limit, release
wakeup, wrong-thread use, allocator failure and reentrant window destruction;
fractional fixtures checked submitted sizes at 1.25x and 1.5x. The same buffer
and fractional tests passed in a Clang shared ASan/UBSan build with leak checks;
the buffer test also passed in a GCC dual-backend build. Private Weston/Xvfb
ran software Settings keyboard, independent AT-SPI error/retry and simple-IME
composition with clean exits; the keyboard case also passed under ASan/UBSan.
The existing Vulkan Settings keyboard case passed separately. These use virtual
input and do not prove physical presentation, a production IME, actual Aetheri,
performance or human accessibility acceptance. Automatic runtime selection
between renderers remains open.

The committed source `4c76394c3bfd9e0c048dc3d57595e5820df1fc77` was then
exported with `git archive` and built as separate static and shared Wayland-only
SDKs with Vulkan disabled. The copied Settings consumer built and installed
outside that source tree with X11 and Vulkan package discovery disabled. Both
SDKs passed private-Weston keyboard and independent AT-SPI error/retry; a
separate shared-SDK run passed `weston-simple-im` composition. All three
fixture results report child, compositor and overall exit 0, and the consumer
probe checked the package/linked-core revision and absence of Vulkan in `ldd`.
Local logs and SHA-256:

- `/tmp/nexaui-software-installed-shared.log`: `3664d776e98af098e1b7d1e9c5c4aee106a25383ea1a7453de25e99fa1be257c`
- `/tmp/nexaui-software-installed-static.log`: `68e769a2661fc35676a044c646a72797366313e15c5ff02f605147205b487df5`
- `/tmp/nexaui-software-installed-ime.log`: `822b0b04e1d5c97ca1cd7b333cde03f811622be85c807ffea5218fccf33938ad`

These remain software-rendered, virtual-input checks; the external and human
gates above are unchanged. No Aetheri consumer pin or backend was changed.

## Scaled software rasterization (2026-11-05)

Candidate based on `5feea514c339718f971fdbabefd3d11367ef699d` adds
`nx_headless_render_scaled` for R01/R03/W04. On Alpine-Dev/WSL2 the reference
parity test and installed C17 consumer passed with a Wayland-only GCC static
build and a Wayland-only Clang shared build with ASan/UBSan and leak checking;
the parity test also passed in a GCC dual-backend build. Vulkan was disabled in
these three builds. The parity case compares all command types at 2x with the
independent scalar reference, checks simple fractional and boundary scales,
and verifies invalid and unrepresentable input leaves the destination intact.
This is not a native Wayland presentation or a software fallback, and no
physical-device, mixed-output or human acceptance is implied. The temporary
command copy uses the default allocator; performance under large streams and
allocation-failure injection remain unmeasured.

Assessment baseline: `05e28bb16c1e60ac339cd9428ff2d3afbafc21f6` (published main, checked 2026-11-04). This is an inventory, not acceptance of the catalogue. The requirements and acceptance criteria remain in the separately supplied `NEXT_AETHERI_UI.md`; this file records the first dependency stage. Later catalogue rows remain to be individually assessed.

The [preliminary full-catalogue source map](aetheri-requirement-map.md) originally
listed 102 requirement IDs; D11 has since been added for the updated 103-ID
catalogue. It lists candidate source families and original acceptance targets.
It is a navigation and triage aid, not a substitute for individual row review or
evidence that the listed acceptance targets have passed.

| IDs | Existing source or original evidence | Disposition and remaining acceptance |
| --- | --- | --- |
| B01 | `CMakeLists.txt` Linux source/link block; `platform/linux/window.c`; `cmake/NexaUIConfig.cmake.in`; `renderers/vulkan/` | Missing implementation: X11 is mandatory in configure, dispatch, renderer and installed package. Tests also link X11. Separate X11 dependencies without dropping Wayland tests. |
| B02-B03 | `CMakeLists.txt`, `cmake/NexaUIConfig.cmake.in`, `include/nexaui/nexaui.h` | Missing profile-specific acceptance: prove static/shared and installed consumers with actual selected dependencies, including AT-SPI and IME. |
| B04-B05 | public `nexaui.h`, `docs/concepts/` | Missing acceptance evidence: inventory version/capability and ownership/error rules against each first-view operation. |
| W01-W02, W04, W10 | `nx_window_create`, `nx_window_poll`, `nx_window_get_info`, `nx_window_route_events`; `docs/concepts/window-management.md` | Existing API; missing composed Settings and selected-compositor evidence, fractional scale and idle/wakeup measurements. |
| R01-R03, R05, R10 | `docs/concepts/rendering.md`, `docs/concepts/renderer-backend.md`, `renderers/vulkan/`, `src/headless.c` | Existing source families; missing consumer-specific renderer/failure/lifetime acceptance. |
| T01-T04, T06 | `nx_text_buffer_*`, `nx_text_field_*`, `nx_window_set_ime_*`, `nx_text_clipboard_for_window`; `docs/concepts/text.md` | Existing API; real installed IME, clipboard and protected-field acceptance for the composed view remain open. Numeric complete native path is separately restricted (T05). |
| L01-L05, L08-L09, L11 | `nx_layout_*`, widget descriptors in `nexaui.h`, `docs/concepts/widgets.md`, `docs/concepts/layout.md` | Existing API families; responsive translated Settings composition and interactions still require a real consumer. |
| I01-I03, I05 | `nx_window_route_events`, `nx_interact`, text clipboard and widget APIs; `docs/concepts/input.md` | Existing paths; keyboard-only, native layout/IME and clipboard behavior in the composed view require acceptance. |
| A01-A02, A06-A09 | `nx_semantics_*`, `nx_window_publish_semantics`, `platform/linux/accessibility.c`, `docs/concepts/atspi.md` | Existing semantic/provider paths; independent native client and screenreader, actual composed controls, contrast and reduced-motion acceptance remain open. |
| D01-D02 | `nx_theme_*`, `docs/concepts/widgets.md` | Existing theme APIs; locale and variable-length translated view acceptance remain open. |
| P01, P04-P05, P08 | `nx_window_poll`, public allocators and lifecycle APIs; `docs/concepts/window-management.md` | Missing composed-view idle measurements and negative lifecycle/diagnostic acceptance. |
| Q01-Q03 | `docs/quality/wayland-private-compositor-report.md`, `tests/check_wayland_results.py` | Prior private Weston evidence is for the revisions named in that report, not this new profile or Settings view. Exact build, runtime library and non-skipped native checks remain required. |

## B01 dependency boundaries

`platform/linux/window.c` includes X11 headers and calls X11 functions throughout the shared dispatch, including geometry and accessibility. The Vulkan adapter and Vulkan/native test targets contain additional X11 references. The exported CMake package currently always calls `find_dependency(X11)`. Merely making `find_package(X11)` optional would leave compilation, linking, installed consumers and fallback semantics incorrect.

The private Weston report uses Weston's X11 backend as a test fixture to provide a virtual seat. This is a test-host dependency and must not be confused with an X11 dependency of the compiled NexaUI profile. A no-X11 SDK build/consumer still needs a separately provisioned real Wayland compositor and cannot claim native IME/AT-SPI acceptance from configure alone.

## First consumer boundary

`examples/04_forms` exercises model and semantic assertions but is not a complete native Wayland Settings application. `examples/10_native_clear/main_linux.c` exercises a native renderer lifecycle but is not a forms/IME/accessibility consumer. Combine existing public APIs in a new consumer; do not treat either existing example as proof of the requested complete view. Aetheri retains its current SDL3 foundation and its own application models while this work proceeds.

## Initial B01 negative control (2026-11-04)

On Alpine-Dev/WSL2, GCC 15.2.0, CMake and Ninja, `CMAKE_DISABLE_FIND_PACKAGE_X11=ON` with Vulkan, tests and examples disabled failed configuration at the mandatory `find_package(X11 REQUIRED)` in `CMakeLists.txt:29` (exit 1). Wayland client 1.25.0, xkbcommon 1.13.1 and wayland-protocols 1.48 were found. `tests/check_wayland_only_package.py` independently reaches the same failure using the proposed `NEXAUI_ENABLE_X11=OFF`; the option is not implemented yet. It is a deliberately failing pre-implementation probe, not a registered default test. Later it checks both static and shared installed consumers, but does not substitute for a build machine without X11 headers, Vulkan-on, native tests, or an actual compositor.

## Vulkan isolation in progress (2026-11-04)

Working tree based on `05e28bb16c1e60ac339cd9428ff2d3afbafc21f6`: `renderers/vulkan/renderer.c` SHA-256 `a4719f64313fff6aa9918ef17dbb43cf2d3ccbe7593bf08388c08f3dd1b2fb9f`. The internal `NEXAUI_VULKAN_WAYLAND_ONLY` compile switch excludes Xlib declarations, surface creation and the X11-only canvas-cache query. It rejects a non-Wayland window before selecting the surface extension. The switch is not yet wired to a public CMake profile.

Alpine-Dev/WSL2, GCC 15.2.0, Release, Vulkan 1.4.347: the existing dual-backend configuration and a configuration with `CMAKE_C_FLAGS=-DNEXAUI_VULKAN_WAYLAND_ONLY` both built `nexaui_renderer_vulkan` (exit 0), and each ran `nexaui_vulkan_device` 1/1. This device test does not cover the modified window-surface path. A separate GCC `-H -fsyntax-only` include trace for the switched renderer contained no X11 header; `nm -u` showed no `nx_window_x11_*` reference in its object, while the default object retained both references. Both whole builds still configured and built the X11 core, so they are not B01 acceptance. Native Wayland presentation and default X11 runtime regression remain open.

## B01 candidate progress (2026-11-04)

Working tree on `05e28bb16c1e60ac339cd9428ff2d3afbafc21f6`, uncommitted. `NEXAUI_ENABLE_X11=OFF` now selects a Wayland-only Linux dispatcher and Vulkan adapter, omits X11 from the installed CMake dependency graph, retains the Wayland native tests, and returns `NX_ERROR_UNSUPPORTED` for an explicit X11 selection. The default is still dual-backend. The package probe passed static and shared Release installed C17 consumers with CMake X11 discovery forbidden (Vulkan/tests/examples off for that probe). This machine has X11 development files, so it is not yet an independent no-X11-filesystem acceptance.

The separate Release Wayland-only build with Vulkan, tests and examples enabled configured and built successfully, including Wayland v1/v3 protocol, Vulkan and accessibility test targets. Its first full build failed because `test_vulkan_draw.c` retained X11-only symbols; that link failure was corrected by isolating its X11 displayed-pixel check, which the previous Wayland run already bypassed, while retaining the Wayland GPU/readback tests. The subsequent full build succeeded. Seven focused tests passed: backend selection, cursor, repeat, wire management, text-input v1/v3 and Vulkan device. They do not establish native compositor presentation or installed IME acceptance. The default dual-backend build passed the two text-input wire tests and the private Xvfb X11 window test (1/1). Runtime X11 Vulkan regression, real Wayland/Vulkan/AT-SPI clients, no-X11-header environment, sanitizer/shared native matrix and independent review are still open. X11-only AT-SPI fixtures are not registered in this profile; Wayland multiselection remains registered, but needs actual execution. No native-test skip counts as success.

The package run preceded later changes limited to test/example CMake and test sources; repeat against the frozen final candidate when the profile is complete. The first composed Settings consumer, the per-ID catalogue assessment beyond the first stage and the remaining delivery stages have not yet been completed.

## Isolated dependencies and private native run (2026-11-04)

The candidate was built on Alpine-Dev/WSL2 with a bubblewrap mount namespace hiding `/usr/include/X11` and masking `libX11.so.6.4.0` and `libX11-xcb.so.1.0.0`. A normal C compiler control built and ran; a separate Xlib header compile and `-lX11` link failed for the expected missing header/library. The static/shared installed C17 package probe passed in this namespace. SDK, Vulkan renderer, native examples and seven selected protocol/device test targets also built. Seven focused CTest cases passed with the same X11 masking after the test namespace made `/tmp` writable. An initial run with read-only `/tmp` failed three `mkdtemp` calls; its original output is retained. A later standalone invocation confirmed both text-input cases with process exit 0. This is a selective filesystem isolation, not a complete independent distribution image.

The full test build in the namespace failed when linking the independent AT-SPI clients: the installed Alpine `libatspi` itself depends on X11. These are test-client dependencies, not NexaUI SDK linkage. No AT-SPI test is counted as passing in the masked environment; the failure is preserved in `out/aetheri-b01-isolation-v1/build.log`. A first setup attempt failed before compilation because `/dev/null` was read-only; its header control was invalid and is not counted. The corrected attempt and all outputs above are separate. The seven-case retry's CTest output says 7/7 passed, but its outer shell returned 1 due to a trailing carriage return from the Windows-to-WSL script pipe; that wrapper error is not hidden.

Separately, the normal Wayland-only Release build ran against private Weston 14.0.2 with its X11 backend on private Xvfb and software rendering. Nine distinct cases executed without skips: four Vulkan presentation/draw/timing/ring cases, native window and window management, independent AT-SPI multiselection, NexaCloud native example and native Vulkan example. CTest returned 0; the fixture recorded child exit 0 and Weston exit 0. JUnit SHA-256 `18648bf87dd50c825022f58f784e2ae6a7b1ec0a9eccd4b8113062e27fe7147e`; fixture result SHA-256 `905e50a30cefa7b87660e2d62c374575040759bf06a7fafc05b29a1cf3250f80`. Evidence is in `out/aetheri-b01-native-v1`; the original JUnit filename contains a carriage return from the shell pipe and the byte-identical `junit-verified.xml` is provided for ordinary access. This is not physical, human, real Aetheri compositor or installed input-method acceptance. The test host intentionally retained X11 for Weston and its libatspi client, even though the SDK was built Wayland-only.

Open: shared/sanitized native runs, default X11 Vulkan regression, review and a final source-frozen complete gate. The Settings application and later catalogue stages remain open.

The default dual-backend Vulkan X11 tests were also run on private Xvfb with software rendering: presentation and draw passed; timing and ring were initially not run because those two executables had not been built. After building only those targets, each passed individually. The initial four-case CTest invocation therefore exited 1 and is not represented as a single 4/4 run; four actual passes were observed across the original and targeted invocations. No physical GPU acceptance follows.

## Settings consumer started (2026-11-04)

`examples/18_settings` adds an application-owned name/reduced-motion draft and
applied model using existing NexaUI scene, text, keyboard and semantic APIs. The
Linux Vulkan runner routes native events and IME/clipboard through the window,
publishes semantics and closes the renderer before scene/window teardown. It is
currently a build-tree example because the Vulkan adapter is private. The
headless `nexaui_settings_scene` test passed on Alpine/WSL2 in the Wayland-only
Release build, checking synthetic Tab/Shift-Tab, text edit, checkbox, Apply,
model/status and invalid-name disabled state. The native Weston/Xvfb software
smoke passed 1/1 with process exit 0 and actual ready frames; after a status-label
correction it was rebuilt and passed again in the separate v2 fixture. These are
not evidence of actual native IME insertion or independent accessibility
operation. The README lists missing resizing, visual, IME and consumer work.

## Settings: independent AT-SPI action (2026-11-04)

An independent libatspi client without NexaUI linkage starts the actual Settings
runner in `--verify-accessibility` mode on private Weston/Xvfb and a private
registry/bus, finds the application by PID and the Display name, Reduce motion
and Apply controls, verifies the checkbox role and unchecked/checked transition,
then invokes Apply. The host checks its applied model contains `Aetheri` and
reduced motion enabled, and exits through renderer/scene/window cleanup. Test
`nexaui_settings_wayland_atspi` passed 1/1, no skip; fixture child and Weston
exit 0. Raw CTest output SHA-256
`657791f166cb84059f41c9274df4fb3c827bad53b8741f1873febdbeafb32ab` and
fixture result SHA-256
`83c714aa079a222a33086bfb065cb26680be1febbd773828a64575a088a6ad3f`
 Original logs are
in `out/aetheri-settings-atspi-v1`. Source baseline remains `05e28bb` with local
changes; scene SHA-256 `43a2a14ca2aca4bec741cd00e2c2d8e15ed5e9e1755c41566526915a50004672`,
native runner `420ef0319a2550b98ca7e83c36b7cd89ce3d46bbaf64dcd37ba2c9beae03a73a`
independent client
`38f042db1ad54ec8a04c164491999d191e9b397502761c18d015ebf713695264`.
This proves these two accessible actions and model acknowledgement, not
accessible text editing, installed IME, a screenreader or human acceptance.

A follow-up tightened the test client's child cleanup: once `waitpid` has reaped
the host, it clears the PID before any failing assertion, avoiding a signal to a
potentially reused PID. The client was rebuilt and the same private native test
passed again (1/1, no skip, exit 0), with scene and host sources unchanged. V2
client SHA-256 `393793c7433f8861d55d70abc85e20fbd29a2b672302df22aeb2682b06756f2f`;
raw CTest log `49d92822c4f8b1851f7c4b18c8dfc3b84e1032dfaff359fb789b02e7110bd763`;
fixture result `bb6bc4533d077304d48755e09985afbed2f712bc60f6988ccc3aa2de0630fc48`.
The v1 result and sources are historical; v2 evidence is in
`out/aetheri-settings-atspi-v2`.

## Settings: Wayland text-input-v3 protocol (2026-11-04)

The existing private Wayland v3 server was extracted to a reusable fixture;
its default 240x120 behavior remains covered by the original unchanged test
body. The new Settings test requests 640x360, focuses the actual display-name
widget and receives native v3 preedit `naï` and commit `café` from that server.
It checks the draft stays unchanged before `done`, the commit appears exactly
once afterwards, the applied model remains `Aetheri`, and preedit clears when
focus moves to the checkbox. The first execution failed at the fixture's fixed
240x120 geometry assertion, before the IME sequence; the server was explicitly
parameterized and the old default preserved. This was a fixture mismatch, not
an asserted SDK defect.

On Alpine-Dev/WSL2, both the original v3 regression and Settings v3 case passed
2/2 in the Release Wayland-only build and 2/2 in Clang 22.1.3 Debug with
ASan/UBSan and leak detection; the separate Settings scene test also passed
under the sanitizers. JUnit files are in `out/aetheri-settings-ime-v1`:
Release SHA-256 `52918fef475479093f553a6677af352b05bb373727e4d7e4a65479549a02f3a6`;
sanitizer SHA-256 `c92de33f62768b1897bdeb4a177ed602dd163f5f75de0f45637b227089b5f8be`.
Settings test source SHA-256 `a16a681437a6bce519c45bad4276ee55df1e27a093692c73af0a929180f38a39`;
shared fixture `36f811219597adcba0f68f5ef556c57b4a0818ea2d5eec594dbaa445a44e4ba5`.
These are real Wayland protocol events against a controlled test server, not an
installed input-method engine, a complete user typing workflow or actual Aetheri
compositor acceptance. Build source remains local changes on `05e28bb`.

## Settings: small view and text capability (2026-11-04)

The settings options now use the existing `nx_scroll_area` and preserve their
IDs while moving focused controls into view. At 240x150 logical pixels a
headless synthetic test exercises Tab/Shift-Tab and verifies the resulting
checkbox/button semantic bounds lie inside the 64-pixel-high viewport after a
following frame. The scene and v3 IME tests passed in Release and in Clang
ASan/UBSan with leak detection. These checks are not a native resize,
long-label, 200% text-scale, visual or human keyboard acceptance. Below 128x120
the native runner still polls without producing a new frame.

On private Weston/Xvfb and private AT-SPI bus, the independent client again
passed the checkbox/Apply/model/clean-exit test. It now additionally reads
`Aetheri` via the provider's `nexaui-value` attribute and verifies Text and
EditableText interfaces are absent. This is positive evidence of a missing
standard text contract, not text-editing acceptance. Raw CTest log SHA-256
`1282c5b0f25536fdeee84355272506fa37310d5029f9350b11960b66c057a522` is in
`out/aetheri-settings-atspi-scroll-v2`. Scene source SHA-256
`5858b9c55bf7fb96f10e2e691e82a3a4c49c8345b50c46a88fdce2987ef2e9bc`;
scene test `496abff1f74974c9be5c2239814890eea5fe922870f740b96c80b16c5ac0c73b`;
AT-SPI client `dd35a42fec6b608c30f5f2c6d921cd7c5c26eaa6f5a86d5a60390f88af030979`.
These working sources remain uncommitted on `05e28bb`.

## Shared Wayland-only native run (2026-11-04)

On Alpine-Dev/WSL2, GCC 15.2.0 Release with `BUILD_SHARED_LIBS=ON`,
`NEXAUI_ENABLE_X11=OFF`, Vulkan, tests and examples enabled, the complete build
succeeded. A private Weston 14.0.2/Xvfb software run selected by `-R wayland`
executed 17 distinct cases, with no skips or failures, including Vulkan, both
Wayland text-input wire versions, AT-SPI multi-selection, Settings native and
its independent AT-SPI client. Two further shared tests, the Settings scene
and its private v3 IME fixture, passed separately. This is not the full SDK test
suite or physical/Aetheri compositor acceptance.

The JUnit evidence is `out/aetheri-b01-shared-v1/results.xml` (SHA-256
`224ba4b0e38089fb6a34f64d17115a59edda2e2c0fd4cfd59a5dcddee985de71`).
A separate actual Settings smoke invocation recorded `/proc/<pid>/maps` while
running, showing the executable mapping from
`/tmp/nexaui-b01-shared-v1/libnexaui.so.0.1.0` (SHA-256
`2ed329fc8e0d2afbc2d6457a8b3467286927d4c814724cf0da7de5a298d99edd`), and
then exited normally; retained `loaded-maps.txt` and fixture metadata are in
the same directory. The Settings executable hash was
`4db609f05bb11bdf83ad1fb286e2dee3d325b88e4ba4f69f6b450537a8fbf915`.
Sources are uncommitted changes on `05e28bb`. The shared run used a normal test
host with X11 libraries for the compositor and libatspi, not the X11-masked
build environment. Independent review, full SDK matrix and native sanitized
Wayland/Vulkan coverage remain open.

## Native Wayland-only ASan/UBSan run (2026-11-04)

Alpine-Dev/WSL2, Clang 22.1.3 Debug, static SDK,
`NEXAUI_ENABLE_X11=OFF`, Vulkan, tests and examples enabled, with
`-fsanitize=address,undefined`, `ASAN_OPTIONS=detect_leaks=1` and
`UBSAN_OPTIONS=halt_on_error=1`: the complete build succeeded. The private
Weston 14.0.2/Xvfb software fixture ran the 17 `wayland`-named tests; all 17
unique JUnit cases passed without skips, including Vulkan present/draw/timing/
ring, v1/v3 input, AT-SPI multiselection and Settings native/AT-SPI. The
fixture and child exited 0. The complete CTest log contains no ASan/LSan,
UBSan or Vulkan validation error marker; validation-layer activation was not
established by this run. It is not the full SDK test suite,
physical input/GPU, actual Aetheri compositor, or proof of installed-IME and
standard AT-SPI text support. The separate instrumented Settings v3/scene
checks are recorded above.

Evidence: `out/aetheri-b01-asan-native-v1/results.xml` SHA-256
`a19677172e4c5669ee578dbb16734c42be2dcb0df1de362960fee3578aab4f18`;
`ctest.log` SHA-256
`12078cd2aab37b740dd54c3527f3c9c424d27b4db2c36d619fc3cbcae820647e`;
fixture `result.json` SHA-256
`a6e66a1fa0459061d08cdb7cd31214767911e67336f7c7a7d4c9639d0256c1c9`.
Source files matched all 24 entries of
`out/aetheri-shared-source-v1/manifest.json` (manifest SHA-256
`ed61c3c8f866fb9314212cbd2d6682fd3c6a482789b899dd36e70c8e85d69bbf`)
before the run; this is a local dirty candidate based on `05e28bb`, not a
published revision. Full SDK matrix, candidate review, no-X11 test-client
dependency isolation and final delivery gate remain open.

## Full locally registered shared suite (2026-11-04)

The Wayland-only GCC 15.2.0 Release shared build ran its complete locally
registered CTest set inside private Weston 14.0.2/Xvfb with Vulkan required.
All 120 registered names appeared exactly once in JUnit; 120 passed, no skips
or failures. The fixture, child and CTest exited 0. This is one local Linux
profile on a virtual software-backed test host, not the complete cross-platform
CI, physical hardware, actual Aetheri compositor or human accessibility
acceptance. The available test-client `libatspi` and Weston themselves retained
X11 dependencies; the SDK under test did not select its X11 adapter.

Evidence under `out/aetheri-b01-shared-full-v1`: JUnit SHA-256
`34cc07705c06129fb759fd892bb6e1285011880054a02ecbcde109c005a51a97`,
registered CTest JSON `41e8c8c53ff32213e414b12fa28c7f215644d33719255367a0510f03b92361da`,
and raw CTest log `581867398539f13754dcdbfc5cbee06a4c0c432cce09d4611236dffbb680e476`.
The 24 source entries in `out/aetheri-shared-source-v1/manifest.json` matched
the current files before the run; baseline commit `05e28bb`, uncommitted local
changes. Previously recorded separate loaded-library evidence remains applicable
to the same build. Independent candidate review and remaining adoption gates
are not implied by this suite.

## Full locally registered sanitizer suite (2026-11-04)

The Clang 22.1.3 Debug static Wayland-only build ran its complete locally
registered set in private Weston/Xvfb with Vulkan, ASan, UBSan and leak
checking enabled. All 121 registered names appeared exactly once in JUnit;
121 passed, no skips or failures, and the fixture/child exited 0. The raw log
contained no AddressSanitizer, LeakSanitizer or UBSan error marker. The one
case registered here but absent from the 120-case shared suite was
`nexaui_selection_set`; this difference is not a missing shared test result.
Neither run represents the whole cross-platform CI or physical, real Aetheri,
installed-IME and human assistive acceptance.

Evidence under `out/aetheri-b01-asan-full-v1`: JUnit SHA-256
`89dae8b016579ec16792a3dc62561c30a0ea4c1329eed2530bfb8d4b78bf9055`,
registered JSON `de31dc0e1c9750adc8528d0d42c4babee8303e410642029ad7b52c46e77e8007`,
and raw CTest log `1ca1598ac190f5319e688f6bc24b4b07b8cca06d27dc7e53ae6cf0eeb16f6a68`.
Source matches the 24-entry candidate manifest noted above, on uncommitted
`05e28bb`-based work. A full independent review and the other platform/build
and consumer gates remain outstanding.

## Settings idle scheduling and native keyboard finding (2026-11-04)

The Linux Settings runner now redraws on routed events or size/scale changes,
keeps retrying a suspended renderer, and polls at up to one second when idle.
If AT-SPI is unavailable it republishes at most once per second to permit the
existing connection retry without redrawing. This does not implement worker
wakeup, a general animation deadline contract, or an idle guarantee for a
permanently suspended renderer. It is an example-level change, not a new public
SDK event-loop API.

Three sequential 3-second private Weston/Xvfb software runs of each version
used the same font and workload. The retained instrumented baseline produced
67/78/74 actual presentations, 68/79/75 poll calls and 4.805/5.304/5.169 s of
process CPU (includes software-renderer worker threads). The measured
candidate produced 1/2/1 presentations, 5/6/5 polls and 0.396/0.201/0.128 s CPU.
These small unpaired samples demonstrate this fixture's behavior only, not a
hardware-wide performance claim. Baseline and measured candidate source and
binary copies, three raw samples and fixture results are retained under
`out/aetheri-settings-idle-v1`. Subsequent current-source changes add only test
modes and their diagnostics; the exact measured candidate is preserved.

An independent AT-SPI client successfully changed and applied the model after
1.2 s of idle. A separate xdotool/Xvfb-to-Weston virtual-keyboard test then
passed Tab traversal, checkbox, Apply, model confirmation and clean close; the
same keyboard-controls, native smoke and AT-SPI tests passed 3/3 under
ASan/UBSan with leak checking. These are not physical-keyboard or human
screenreader acceptance.

The stronger registered `nexaui_settings_wayland_keyboard` text test remains
**failing** on this private Weston v1 setup: it times out waiting for the model
to contain the keyboard-entered `Ada`. Wayland debug records keyboard focus and
all key events, while the settings diagnostic records an unchanged 7-byte draft;
the checkbox and Apply action do commit. The protocol trace shows text-input-v1
entered and language/direction replies but no commit-string for the typed keys.
The adapter currently suppresses ordinary xkb text while IME is entered; the
trace alone does not establish whether the compositor/input-method configuration
or SDK policy is the ultimate cause. A diagnostic run forcing the pre-existing
continuous redraw behavior also failed with the same unchanged draft, so the
new idle strategy is not necessary for reproducing the observed failure. Earlier
fixture attempts failed to locate an untitled Weston X11 window; those did not
send input. The test now uses the private compositor's recorded window ID.

Original logs include `keyboard-diagnostic-v1.log` SHA-256
`86ddb08415ad83a67ab4c3c5822bdc92b9d43065c1a20de4a57c5a3c989a8790`,
`keyboard-continuous-v1.log`
`691fc6462c6daac101aae916fd48b9a5051e541dc569ef6974b20a56bb2f2bcb`,
`keyboard-text-final.log`
`fd50b94bf90fc2cf5189b128bca7d0f332b5fa42b2c02acafae37768d8c1452a`
and `keyboard-controls-v1.log`
`656c3894fe8d7686503c5c0d124284e0bd6d168f4774381a062851d0d523cba1`, and
`asan-controls-v1.log`
`9aec8131f94d3e12649a00170ccea8279eba2111f451fc5bd443c4ba2bf46de4`.
The 120/121 full-suite results above are historical for the previous source;
they do not include this new failing regression or certify the current tree.
Current work remains uncommitted on `05e28bb`.

## Forwarded hardware input and installed input method (2026-11-04)

The earlier default-Weston keyboard-text failure above is now fixed: entering
text-input does not imply that the input method grabbed the hardware keyboard.
The adapter emits ordinary xkb text for delivered `wl_keyboard` keypresses,
while independently handling input-method commits. A regression verifies the
delivered key/text pair and a separate composed commit. The native Settings
keyboard text and controls tests pass with the private Weston screen keyboard;
the v1 and v3 protocol fixtures also pass. This is not evidence for all IMEs.
The input-method-v1 protocol describes the input method's optional keyboard
grab and forwarding of keys it does not process:
https://raw.githubusercontent.com/wayland-mirror/wayland-protocols/1.48/unstable/input-method/input-method-unstable-v1.xml

An additional private Weston 14.0.2 run launched `weston-simple-im` and failed
both Settings keyboard checks. Its recorded protocol trace shows the initial
Tab as `wl_keyboard`, but while the field is active the input method sends
`commit_string("a")` for Ctrl+A and `commit_string` containing Tab and space
for later navigation. The example consequently cannot perform the expected
selection and focus traversal in that setup. This observation is specific to
the test input method and configuration; it does not establish behavior of
production IMEs or the Aetheri compositor. No events were synthesized to hide
the failure. The fixture supports selecting this input method explicitly; its
default remains the Weston screen keyboard. Raw results and trace are in
`out/aetheri-ime-forwarded-v1-simple-im.xml` and
`out/aetheri-ime-forwarded-v1-simple-im-trace.xml`, with compositor logs in
the corresponding `out/` directories. Real IME, physical input and human
assistive-technology acceptance remain open.

On the revised candidate, the complete locally registered shared Wayland-only
suite passed 122/122 in the default private Weston/Xvfb fixture, including the
new native keyboard cases (fixture and child exit 0). This supersedes the
historical 120-case shared result above for this candidate, but does not include
the separately failing `weston-simple-im` variant or the remaining acceptance
work. Evidence: `out/aetheri-ime-forwarded-v1-shared-full.log`, its adjacent
JUnit XML and the fixture's `result.json`.

The corresponding Clang Debug static Wayland-only run with ASan, UBSan and leak
checking passed 123/123, with child and compositor exit 0. The additional case
relative to shared is `nexaui_selection_set`. Its log contains no sanitizer or
UBSan error marker. Evidence: `out/aetheri-ime-forwarded-v1-asan-full.log`, its
adjacent JUnit XML and the fixture's `result.json`. Both full suites used
virtual input and the default Weston input method; the separately documented
`weston-simple-im` failure is not part of their pass count.

## Current Linux consumer profiles (2026-11-04)

On the same uncommitted `05e28bb`-based candidate, an external project copied
the existing public C17 package consumer into a temporary directory, used
`add_subdirectory` and linked only `NexaUI::NexaUI`. All four combinations of
static/shared core and Vulkan off/on configured, built and ran with
`NEXAUI_ENABLE_X11=OFF` and CMake X11 discovery forbidden. The two shared runs
also used `ldd` with `LD_LIBRARY_PATH` cleared and resolved the library under
their own build directories. The Vulkan-enabled builds built the private Vulkan
target; this headless consumer does not link it or test presentation. The
reusable probe is `tests/check_wayland_only_add_subdirectory.py`, SHA-256
`ef84a0204b2d14271ca814198398e0647db44d01f86cc525773abcf52afa31fd`;
`out/aetheri-b03-add-subdirectory-v2.log` SHA-256
`2fe896cd6afbb752b3907ea79fef54e5dff935f87fab34735a9ce6f25bd08815`.

The separate existing installed-package probe passed static and shared C17
consumers again with Vulkan off. It also passed in a bubblewrap namespace
hiding the X11 headers and masking the installed X11/X11-xcb shared library
files: a normal C executable compiled and ran, while dedicated X11 header and
link controls failed as expected. The namespace is selective isolation on this
host, not a separate minimal distribution. Its probe log is
`out/aetheri-b03-isolated-current-v1.log`, SHA-256
`0fe05e46645adae223a37401d44ffa06662751f3c680020aa302449a017e49ac`;
the ordinary-host log is `out/aetheri-b03-installed-current-v1.log`, SHA-256
`d010c671c326fc68f886d1c7ddd07db00408a83440f295215c6c89c2f901dcc8`.
All three processes exited 0. These builds used Alpine-Dev/WSL2, GCC 15.2.0,
CMake 4.2.3 and Python 3.14.8; Vulkan-enabled builds found 1.4.347. They do not
prove native compositor, installed IME, AT-SPI, independent-distribution,
default-X11 or other-platform acceptance, and do not close B03/Q05 in full.

## Settings focus visibility after resize (2026-11-04)

The scene previously revealed the focused control only when its focus ID
changed. A deterministic scene regression focused Apply at 640x360, rendered
another frame at that size, and then resized to 240x150 without input. The
original code failed the focused semantic-bounds check in
`out/aetheri-settings-resize-before-v1.log` (exit 8; SHA-256
`ba857a0e7b6001936e42b078be7368878e6f065fcd35e0f4c75ac120dc0610d5`).
The scene now reevaluates the same focus when viewport height changes. The
native host also schedules a bounded additional frame after a focus change so
that the scene can reveal the new target without waiting for another input.

The new private-Weston keyboard case requires the actual client dimensions to
be 240x150, holds each Tab key down until the focused control's published
semantic bounds fit the viewport, then completes text edit, checkbox and Apply.
All seven Settings scene, controlled v3, virtual keyboard, native and AT-SPI
tests passed without skips under both the Wayland-only GCC build and the Clang
ASan/UBSan build with leak checking. The raw logs are
`out/aetheri-settings-resize-final-v2.log` (SHA-256
`29514baa0c24e00c511bacf41ce9373b573f472aa593c63c306a2e944d294167`) and
`out/aetheri-settings-resize-asan-v2.log` (SHA-256
`708175772c3fb5557ba69d6eab4326f390d79777c543d3f464919ff20d571190`).
The final focused reruns record visible-focus IDs 3, 4 and 5 in
`out/aetheri-settings-visible-final-v1.log` and
`out/aetheri-settings-visible-asan-v1.log`; both fixtures and child processes
exited 0. These check published geometry and virtual input, not rendered pixels,
physical keyboard or human accessibility acceptance.

The first native test required a frame with zero routed events; that assumption
failed because the fixture delivered additional events. Its failure is retained
in `out/aetheri-settings-small-v1.log`. The replacement waits for observed
visible geometry before proceeding. A further negative control removing only
the host's focus-change follow-up also passed in this fixture
(`out/aetheri-settings-small-no-followup-v1.log`): native events can supply
another frame, so this test does not prove the follow-up independently. The
headless resize regression above does reject the original scene behavior.
The full 122/123-test suite results earlier in this document precede this
change; only the focused checks above were rerun on it.

## Idle accessibility recovery and waiting (2026-11-04)

Candidate based on `cebf6ce2201da564a75f1555149d7afc44bf037a`. A disconnected
AT-SPI bridge previously reconnected only when the application published a new
semantic frame. An independent libatspi client now pauses its host's frame
production, disconnects the transport, and requires rediscovery of the same
snapshot under a fresh bus identity before resuming. The base failed that
rediscovery. The candidate retries during window polling and reduces the
owner's two-second poll wait to the existing one-second retry deadline; removing
that timeout reduction made the bounded recovery test fail. The independent
client also verifies that the old reference is defunct and the fresh target
works after the host resumes.

The Settings host now requests a long event-driven wait when idle and AT-SPI is
available. A three-second private-Weston test observes at least one actual
one-second-or-longer wait; the previous fixed one-second poll failed this check
with 6 polls, one presentation and 0.137 s process CPU in that run. The
candidate produced 4 polls, one presentation and 0.163 s CPU in the GCC run,
and 4 polls, one presentation and 0.208 s CPU under ASan/UBSan. These single
samples demonstrate this fixture only. A disconnected or unavailable provider
retains bounded retries; synchronous AT-SPI address discovery can still take
up to 250 ms per attempt. General worker wakeup, animation scheduling,
minimized-window measurements and actual screenreader acceptance remain open.

The complete private-Weston suites on this candidate passed 172/173 with GCC
and 173/174 with ASan/UBSan. Both failures were the previously documented
intermittent `nexaui_settings_wayland_atspi_clipboard_once`; a GCC isolated
rerun also failed. A comparison restoring only the former one-second Settings
poll interval failed that same case in three more attempts. This comparison
does not prove the ultimate cause or turn the failures into passes. The native
X11 AT-SPI recovery and X11 window tests passed on a separate dual-backend
build. The one-shot failure remains open.

## Text-input-v1 keysym modifiers (2026-11-05)

Candidate based on `a74f206b855e0587d291110a74d243f98789e0f6`: the v1
adapter ignored both `modifiers_map` and the keysym's modifier mask, instead
using the unrelated physical-keyboard state. A controlled server sent
Control+A using a reordered map while the physical state was unmodified; the
new test failed against the previous source and passed after translating the
protocol map. It checks Shift, Control, Mod1/Alt, Mod4/Super, changed maps,
bit 31, unmapped bits, stale serial, invalid key state, malformed map and
recovery. Events referring to unmapped bits and all keysyms after a malformed
map are discarded; unknown names do not map to NexaUI modifiers. The
[text-input-v1 protocol](https://gitlab.freedesktop.org/wayland/wayland-protocols/-/blob/1.48/unstable/text-input/text-input-unstable-v1.xml)
defines map positions as keysym-mask indices and separates key events from
ordinary committed text. The fixture also routes Control+A into a focused
text field and verifies selection of the whole draft. Its first attempt sent
the key in reply to an older queued state commit and was correctly rejected as
stale; the final fixture first completes those pending requests.

Wayland v1/v3 controlled tests and three Settings v3 scene tests passed in the
GCC shared build and Clang ASan/UBSan static build. Four Settings keyboard
cases passed on private Weston with its default input method. An intermediate
candidate before the final unmapped-bit guard was also run with
`weston-simple-im` and failed all four: two timed out waiting for applied
state, one did not reach the next visible focus, and one did not reach the
expected clipboard draft. Its output is retained locally in
`/tmp/nexaui-ime-modifiers-simple.log`, SHA-256
`095122871a7207831a87fafb3c6a3ad1d585644f5f3276ce9027ac5baa2c5462`.
The earlier trace above shows that this input method commits control keys as
text in that setup; this keysym correction does not solve that separate
behavior. Real installed-IME and human acceptance remain open.

## Real simple-IME compose in Settings (2026-11-05)

Candidate based on `7253567b81b3f07570256cb06775d0a3f3a1dab9`. A private
Weston 14.0.2/Xvfb run with `weston-simple-im` and a private `compose:ralt`
keymap sent Compose, apostrophe and `a` to the actual Settings window. The
test waited for the focused field and an apostrophe preedit while both its
draft and applied model still contained `Aetheri`, then for a cleared preedit
and the committed `Aetheriá` draft while the applied model remained unchanged.
It checked the actual text-input-v1 preedit and commit wire messages, and the
host exited through renderer, scene and window cleanup. The test passed once
with GCC shared and once with Clang ASan/UBSan static. A later GCC run retained
the Wayland trace in ignored local artifact
`out/aetheri-real-ime-20261105-trace.txt`, SHA-256
`d78272853b4951495009e52642397da1358c9dd1d4f1f02e79082849f6f3fa73`.
Replacing the final
`a` with `b` in a private test-driver copy caused the commit expectation to
fail; its local output `/tmp/nexaui-real-ime-negative.log` has SHA-256
`433d28fc8c7adb9a699a4e6a9bad1a70f04c335316ff1f888f414d545ece65d2`.
The new CTest skips outside its explicitly enabled private fixture.

The first exploratory run misplaced the desktop-shell configuration and Weston
did not start; the corrected run launched, but the Xvfb server reset its keymap
after the setup client disconnected, yielding two ordinary text commits. A
private Xvfb with `-noreset` kept the mapping and produced genuine preedit and
commit. The `setxkbmap` executable was extracted into `/tmp`, without a system
installation. This narrow virtual-input check does not resolve the separate
`weston-simple-im` keyboard-workflow failures, nor prove focus-loss
cancellation, production IMEs, visual sharpness or human accessibility.
The [Weston 14.0.2 simple-IME source](https://github.com/wayland-mirror/weston/blob/14.0.2/clients/simple-im.c)
documents the test input method's compose table and event handling.

## Simple-IME cancellation on window focus loss (2026-11-05)

Candidate based on `30a922b20196a451ebc9d5e5947c67da18530f1d` adds an
opt-in real-input focus-loss case alongside the compose test. Once the
apostrophe preedit is active, a separate, disposable X11 window on the private
Xvfb server takes focus. Settings must observe the preedit clear with its
`Aetheri` draft and applied model unchanged. After focus returns, the test
waits for a new Wayland keyboard enter and text-input enter, types `a`, and
requires draft `Aetheria`, no stale `á` commit, unchanged applied model and
normal cleanup. The saved trace confirms preedit, keyboard leave, text-input
deactivation, keyboard and text-input re-entry, and the plain `a` commit in
that order. This tests focus loss between windows, not between controls.

Both real-IME tests passed on private Weston 14/Xvfb in the GCC shared and
Clang ASan/UBSan static builds; the four ordinary Settings keyboard tests also
passed with Weston's default test input method. In a private negative-control copy, focusing
Weston again instead of the peer made the cancellation expectation time out.
The negative log is `/tmp/nexaui-ime-cancel-negative.log` (SHA-256
`487166efb9f5213ac374093a5a3b7cc06a6868b4b60563603f8dde65c834de96`);
the deliberately interrupted test fixture logged a broken pipe on teardown.
The final GCC trace is retained in ignored local artifact
`out/aetheri-ime-focus-cancel-20261105-trace.txt` (SHA-256
`ee9bcad8cbfaa6119cfc965350c74ad64b816ec021d8250c53f3d23436aa813f`).
The previously documented `weston-simple-im` keyboard workflow, production
IME variants, mixed-scale focus changes and human acceptance remain open.

## Text-input-v1 commit after control focus change (2026-11-05)

Candidate based on `e6baeb4674cfa6c52339f94e908b28b6a19b497a` adds a real
`weston-simple-im` test that changes focus from the Settings text field to its
checkbox and back through a separate libatspi `GrabFocus` client. It checks
preedit cancellation, unchanged draft/applied model, no window keyboard leave,
and a subsequent plain `a` inserted into the correct field. Its first run
failed: the input method responded to state serial 5 after a cursor-rectangle
update had advanced the client to serial 6, so the existing strict equality
check silently discarded the text. This trace is retained in
`/tmp/nexaui-ime-control-first.log` (SHA-256
`e7a125b593a6d092b55b972f60b3924b1648eaba9f947efeb9a32ce6d34f9d38`).

The v1 commit handler now accepts an older serial only within the current
activation and while it still refers to the currently advertised editor and
surrounding-text revision. Changed or missing snapshots do not bypass their
existing checks. A controlled server reproduces the caret-only race; it failed
on the previous code and passed after the fix. The final test includes a
delete-and-commit pair and confirms that a later changed revision or different
editor still rejects both events. The
final real-client trace records the serial-5 commit after the serial-6 caret
update, followed by the correct `Aetheria` surrounding text. It is retained in
ignored local artifact `out/aetheri-ime-control-20261105-trace.txt`, SHA-256
`e4d10dffa359801a7fc1752ebe39419c0f6539bcb97b2c1c6b8a3a12b250ad87`.

All three opt-in real simple-IME cases passed with GCC shared and Clang
ASan/UBSan static. Controlled v1/v3 tests and three Settings v3 tests passed
in both builds; the v1/v3 pair also passed in the dual-backend build. The
separate simple-IME keyboard workflow documented above still has failures;
other input methods, mixed-scale focus changes and human acceptance remain open.
The full GCC private-Weston run passed 200 executed tests; the three opt-in
simple-IME cases were skipped in that run and passed separately as above. That
full run preceded the final test-only delete-pair extension; the extended v1
test then passed independently in GCC shared, Clang ASan/UBSan static and the
dual-backend build. Full-run log `/tmp/nexaui-ime-control-full.log` has SHA-256
`d99c1f1619b2bc5772822ee19014b12af0ede1cf4ef9c6af7b5edba2b01a6d1c`.

## Real IME replacement of an accessible selection (2026-11-05)

Candidate based on `054cf3bb1f85a2c985ea29679bece1ae230bf853` adds an
opt-in private-Weston test that selects characters 1 through 4 of `Aetheri`
through a separate libatspi client, composes `á` with `weston-simple-im`, and
checks the unchanged applied model, intermediate preedit, resulting `Aáeri`
draft and clean exit. The initial test failed: a delayed v1 `leave` from the
previous activation cleared the already published new surrounding snapshot,
causing the next update to reset the composition. The v1 handler now keeps
the new snapshot when that leave arrives before the new activation's enter.
The initial failing log is `/tmp/nexaui-ime-replace-first.log`.

All four opt-in simple-IME cases passed separately with the GCC shared and
Clang ASan/UBSan builds on private Weston 14.0.2/Xvfb. Logs are
`/tmp/nexaui-ime-replace-second.log` and `/tmp/nexaui-ime-replace-asan.log`.
The full GCC suite recorded 198 passes, four opt-in skips and two failures
(`nexaui_atspi_idle_reconnect_wayland` and
`nexaui_settings_wayland_keyboard_clipboard`) while another native fixture was
running; both failed cases passed in a subsequent isolated run. The cause of
the initial failures was not established. Logs are `/tmp/nexaui-ime-full2.log`
and `/tmp/nexaui-ime-retry.log`. The skipped IME cases passed separately as
described above.
These automated tests do not establish acceptance with other input methods,
physical hardware or a human user; the keyboard workflow limitation above
also remains open.

## Deterministic v1 leave ordering (2026-11-05)

On source `040fa777671d47e9565010e660dcc30223ba82bb` plus the test change,
the controlled Wayland v1 server queues a deactivate/activate pair without
dispatching the old leave first, sends new preedit, and checks that publishing
the unchanged surrounding snapshot does not erase it. It then sends a real
leave after the new enter and verifies that preedit is cancelled and a later
commit is rejected. This exercises both sides of the leave distinction
without relying on Weston timing.

With the prior leave guard temporarily removed, this test failed at the
preserved-preedit assertion (`/tmp/nexaui-v1-leave-negative.log`); the guard
was restored before the final builds. The v1 and v3 controlled tests both
passed in GCC shared Wayland-only, Clang ASan/UBSan static Wayland-only, and
GCC static dual-backend builds. This supplements, rather than replaces, the
actual simple-IME and independent AT-SPI evidence above.

A subsequent full GCC Wayland-only suite without a second native fixture
running concurrently reported 198 passes, four opt-in skips and two different
failures: `nexaui_settings_ax_async_short` and
`nexaui_settings_wayland_atspi`. Both passed when rerun in a fresh fixture.
The 100 ms async timeout and the AT-SPI clipboard error assertion were involved,
but the causes have not been established; no deadlines or assertions were
weakened. The logs are `/tmp/nexaui-ime-full-isolated.log` and
`/tmp/nexaui-ime-leave-retry.log`. These results do not establish a clean full
suite or explain the earlier two failures.

## Accessible paste timeout test ordering (2026-11-05)

On source `3e3fcaf1fc3361c6d3fd6197f2fec9fc8084a9cd` plus this test
change, a controlled delayed-focus case waits beyond the existing 100 ms
deadline and dispatches the timeout before the focus frame. The previous test
failed at its assertion that the request must still be pending
(`/tmp/nexaui-ax-delay-before.log`). The test now accepts either a pending
request or one already completed as a non-applied timeout, and still requires
exactly one completion, released ownership, unchanged text and the original
upper time bound. The delayed case additionally requires that timeout to have
actually completed before the assertion. This reproduces a valid ordering with
the same symptom as the previous intermittent failure; it does not prove the
historical run followed that ordering.

All eight accessible asynchronous paste cases passed in the GCC shared and
Clang ASan/UBSan Wayland-only builds before the delayed-case assertion was
strengthened; the normal and strengthened delayed cases were then rerun and
passed in each build. No product timeout or behavior was changed. The separate
native AT-SPI clipboard-serial failure remains under investigation.

The full GCC Wayland-only private-Weston suite after the timeout test change
passed 201 executed tests; four opt-in simple-IME tests were skipped there
(`/tmp/nexaui-ax-short-full.log`). This run preceded the resource-lock change
below; the opt-in IME cases had passed separately on the preceding source.

## Shared native-seat test isolation (2026-11-05)

Running `nexaui_settings_wayland_atspi` concurrently with
`nexaui_settings_wayland_atspi_clipboard_peer` on the same private Weston seat
reproduced both the missing-error assertion previously seen in the first test
and a focus-restoration failure in the peer test (`/tmp/nexaui-atspi-pair.log`).
The first test assumes no input serial; the peer test sends actual key events
and changes focus on that same seat. The former now uses the existing CTest
`native_display` resource lock already held by the latter. This serializes
these competing native-seat fixtures without changing either test's behavioral
assertions. It does not establish the cause of unrelated historical failures.
After adding the lock, the same two-test selection with `-j 2 --repeat
until-fail:6` ran each case six times successfully, with the seat-dependent
cases serialized (`/tmp/nexaui-atspi-pair-lock.log`, SHA-256
`8c6b27be3ec1fde7b21722132d7d0191210d850539f26ae79c7b6565f323f213`).

## Settings submission and example-backend failure (2026-11-05)

Candidate based on `cc2d39da75cb1eab9fe954c54cbf2df81e882612` adds an
application-owned, single in-flight submission to the Settings example. Apply
captures the name and reduced-motion choice with a nonzero token; its displayed
model changes only after a matching successful host completion. Stale and
duplicate completions are rejected. The draft remains editable during the
operation, and a later draft does not change the submitted snapshot. Pending,
failure, validation and retry are exposed through the existing status semantics;
the Apply control is disabled during the operation. Focus is restored after
completion only if it was lost from Apply and the user has not focused another
control. The native example host uses a 250 ms owner-loop timer and can fail the
first request with `--simulate-error`; this is a local deterministic example
backend, not a real Aetheri or operating-system operation. Closing the scene
discards the pending local work.

The scene test covers pending state and disabled Apply, unchanged applied model,
stale/duplicate replies, failure, retry, modified draft, snapshot application
and focus retention. A separate libatspi client observed the failure status,
retried Apply, and observed normal host completion on private Weston/Xvfb;
the ordinary AT-SPI case passed in the same two-case run
(`/tmp/nexaui-apply-atspi.log`). Four private-Weston keyboard cases passed
(`/tmp/nexaui-apply-keyboard.log`). The scene and 11 adjacent IME/accessibility
cases passed with Clang ASan/UBSan before the final validation-message priority
adjustment; final focused reruns are recorded below. Human screenreader and
visual acceptance, resizing/long-label reflow, real service integration and
worker-thread wakeup remain open. This advances the first-view L04/A02/P07
scenarios without claiming their entire catalogue rows are complete.

The complete GCC Wayland-only private-Weston suite passed 202 executed tests
with four opt-in simple-IME skips (`/tmp/nexaui-apply-full.log`) before the
final validation-message priority adjustment and its added assertion. On the
final candidate, the scene test passed again in GCC shared and Clang ASan/UBSan;
the native AT-SPI failure/retry and keyboard cases both passed
(`/tmp/nexaui-apply-final-native.log`). All four opt-in real simple-IME cases
then passed separately (`/tmp/nexaui-apply-simple-ime.log`). These are virtual
fixture results, not physical-device or human acceptance. The native AT-SPI
failure/retry case also passed with Clang ASan/UBSan and leak detection
(`/tmp/nexaui-apply-asan-native.log`).

## Narrow Settings layout and status scrolling (2026-11-05)

Candidate based on `120384bcb9d80f9b5a958bab337fbfecbb30ca7a` replaces the
fixed 500-pixel status layouts with cached layouts for the actual content
width. It measures the status height for semantics and scroll extent, fits the
Apply control to the column, and reflows the title, field and motion labels;
the motion label moves below the checkbox at narrow widths. Fixed labels with
an unbreakable word wider than the column are reduced just enough to fit.
Focused controls are revealed again after a width change. Page Up/Down while
the checkbox or Apply is focused moves by half a viewport, keeping successive
pages overlapping. It does not consume text-field paging keys.

The earlier implementation failed the new minimum-width Apply-bounds check
(`/tmp/nexaui-status-reflow-before.log`). A native test first revealed that
whole-viewport page jumps could skip completely over a short status; the
recorded positions in `/tmp/nexaui-reflow-debug.log` motivated overlapping
steps. The headless scene test checks the 128-pixel Apply bounds, measured
multiline status height, keyboard paging, focus and width-only resize; it
passed in GCC shared and Clang ASan/UBSan. Private Weston/Xvfb tests exercised
keyboard navigation at 128x150 and 240x150, reaching the status at minimum
width, plus the native AT-SPI failure/retry case
(`/tmp/nexaui-reflow-complete.log`). This is not a visual or human screenreader
review. Translations, 200% text scale, 128x120 label visibility and physical
display acceptance remain open.

Before the final test-only adjustment for fonts whose unbreakable words require
scaling, the full GCC shared Wayland-only private-Weston suite
passed 203 executed tests with four opt-in IME skips
(`/tmp/nexaui-reflow-full.log`). All four skipped real `weston-simple-im`
cases then passed separately (`/tmp/nexaui-reflow-ime.log`). The adjusted scene
test passed again in GCC shared and Clang ASan/UBSan.

## German/English Settings example and catalogue update (2026-11-05)

Candidate based on `8d82652c08684c792ed36b28ed207dde3236a345` adds a bounded
application-owned DE/EN string table to the existing Settings scene. `--de`
selects German in the native example; the scene's `settings_set_language`
rejects other codes and reflows the same controls with stable semantic IDs.
The example also allows `--de --simulate-error`. The scene test changes
language around a UTF-8 draft and an in-flight submission and checks focus,
semantic names/status, unchanged token and correct eventual model. German
labels use the same width-aware layout as English, including its documented
small-size fallback for unbreakable words. General pluralization, locale-safe
formatting, native window-title changes after a live scene-language switch,
translated IME composition, visual legibility and human screenreader review
remain open; this does not complete D02/L02 or 200% text-scale acceptance.

The private Weston keyboard test passed in German at 240x150 and 128x150; an
independent AT-SPI client found the German names, observed a German simulated
failure and retried successfully (`/tmp/nexaui-de-native.log`,
`/tmp/nexaui-de-min.log`). The scene test passed with GCC shared and Clang
ASan/UBSan. The complete GCC suite passed 206 executed cases and skipped four
opt-in IME cases (`/tmp/nexaui-de-full.log`) before a final status assertion and
argument-parsing extension. After those changes, the scene and two selected
German native cases passed again (`/tmp/nexaui-de-final.log`), as did all four
separate opt-in simple-IME cases (`/tmp/nexaui-de-ime.log`).

The updated external catalogue SHA-256 is
`4e75bfc2c040c2eae74dfd164ec3ec33482af0b87671237470d4f8e7a13a808e`.
An ID comparison found D11 missing from the earlier 102-row map; the map now
contains 103 matching IDs with no extra IDs. The new concrete integration
cases and changed acceptance wording still need individual reconciliation;
ID parity is not a claim of full requirement review or acceptance.

## Linux application-owned wake descriptor and Settings worker (2026-11-05)

Candidate based on `f02f7229ea06c038af450564acec894aa6dccdeb` extends the
existing Linux owner-thread poll path with `nx_window_poll_with_fd` and the
`NX_BUILD_LINUX_POLL_FD` capability bit. The descriptor is borrowed only for
the call; readable, hangup and error are reported separately, and the SDK
neither drains nor owns it. Both Wayland and X11 retain the existing native and
AT-SPI polling. The Settings example replaces its timer-only completion with
one short-lived worker and a fresh pipe for one correlated fixed-size reply;
the worker never receives UI/window objects. The owner reads and applies the
reply; close joins the example worker before releasing the pipe and scene.
This advances W10/I07/L04/P05 but is not a general service or reconnection
implementation. Long-running work, multi-message backpressure, multiple fds,
other platforms, ThreadSanitizer and real Aetheri service acceptance remain open.

The FD test checks an empty wait, worker wakeup, ownership, wrong-thread calls,
invalid descriptors and hangup. It passed with the real private-Weston Wayland
backend and private-Xvfb X11 backend. Settings keyboard and independent AT-SPI
failure/retry tests passed with the worker; a native test also exits while the
worker is still pending. These three Wayland cases passed again with Clang
ASan/UBSan and leak detection (`/tmp/nexaui-worker-final-asan.log`). Static and
shared installed C17 consumers built and ran with X11 discovery disabled,
including the new symbol and capability (`/tmp/nexaui-worker-package.log`).

The first full run reported 207 passes, four opt-in skips and one AT-SPI focus
failure (`/tmp/nexaui-worker-full.log`); two native AT-SPI fixtures were active
on the same compositor. The multi-selection and large-text fixtures now use
the existing native-display lock. The three affected AT-SPI cases then passed
in a serialized repeat (`/tmp/nexaui-worker-lock.log`); the original focus
failure is retained and its exact cause is not asserted as proven.

The subsequent full GCC shared Wayland-only run with those locks and the
capability bit passed 208 executed tests, with four opt-in IME skips
(`/tmp/nexaui-worker-full2.log`). After a final EAGAIN-handling adjustment and
an additional combined data/hangup assertion, the FD case passed again on
Wayland and X11; the Wayland FD, accessibility retry and pending-close cases
also passed with ASan/UBSan (`/tmp/nexaui-worker-last-asan.log`). All four
previously skipped simple-IME cases passed separately on the final candidate
(`/tmp/nexaui-worker-last-ime.log`). The full suite was not rerun after that
final small adjustment.

## Public Linux Vulkan consumer (2026-11-05)

Candidate based on `2c530da4d532f5d680b44563e5a10aacb0824768` adds the
optional installed `NexaUI::Vulkan` target and opaque `nexaui/vulkan.h` view
over the existing private renderer driver. Settings now uses this public
contract, with no private renderer headers or private include directories in
its host target. The caller owns the window, submits logical commands at a
declared physical size/scale, handles suspended and lost outcomes, and retries
close without discarding the window lease. Wrong-thread close preserves the
view. The initial public view requires strict present retirement; the private
compatibility policies are not yet exposed through this contract.

Separate C17 consumers linked to the installed static and shared-core packages
and each submitted a frame on private Weston/Xvfb with software rendering.
They used only the installed include directory and public target; fixture
results `/tmp/nexaui-vulkan-last-static-20261105/result.json` and
`/tmp/nexaui-vulkan-last-shared-20261105/result.json` record child and Weston
exit 0. They also checked malformed commands, zero-size suspension, duplicate
window lease and wrong-thread close. An add_subdirectory consumer submitted a
frame in `/tmp/nexaui-vulkan-subdir-native-20261105/result.json` before the
final output-label-only test edit. A core-only consumer configured and ran with
Vulkan discovery disabled even when the package included Vulkan. A package
built without Vulkan rejected the required Vulkan component in both static and
shared runs (`/tmp/nexaui-vulkan-package-off-verified.log`).

The full GCC shared private-Weston suite passed 209 executed tests and skipped
four opt-in IME cases (`/tmp/nexaui-vulkan-public-full.log`) before an explicit
public-outcome mapping adjustment. After that adjustment, the public consumer,
Settings keyboard and AT-SPI error/retry tests passed again
(`/tmp/nexaui-vulkan-last-native.log`), as did the public consumer and Settings
close/retry cases under ASan/UBSan (`/tmp/nexaui-vulkan-last-asan.log`). All
four opt-in simple-IME cases also passed (`/tmp/nexaui-vulkan-last-ime.log`).
These results demonstrate software submission, not physical presentation,
Aetheri/wlroots behavior, a complete installed Settings application, GPU driver
coverage or renderer packaging for other platforms. Those gates remain open.

The renderer archive is now built as position-independent code. With a shared
core, the installed consumer also built and loaded a separate shared component
that links this archive before submitting its frame
(`/tmp/nexaui-vulkan-pic-plugin-20261105/result.json`); the installed static
consumer also submitted a frame (`/tmp/nexaui-vulkan-pic-static-20261105/result.json`).
After that build change, the public consumer and Settings pending-close and
AT-SPI retry cases passed under ASan/UBSan
(`/tmp/nexaui-vulkan-pic-asan.log`). The earlier full suite predates the PIC
build change; separately executed renderer regressions are recorded below.

Four existing native Wayland Vulkan draw, present, timing and ring cases passed
on the PIC build (`/tmp/nexaui-vulkan-pic-regression.log`). The first attempt
used two nonexistent build-target names and ran no tests
(`/tmp/nexaui-vulkan-pic-regression-build.log`); the corrected build used the
actual target names.
The four opt-in simple-IME cases also passed on the PIC build
(`/tmp/nexaui-vulkan-pic-ime.log`).

The final consumer CMake files link only `NexaUI::Vulkan` directly; they do not
independently locate or link Threads, Vulkan or Wayland. Installed shared,
installed static and add_subdirectory versions each submitted on private
Weston (`/tmp/nexaui-vulkan-direct-shared-20261105/result.json`,
`/tmp/nexaui-vulkan-direct-static-20261105/result.json`,
`/tmp/nexaui-vulkan-direct-subdir-20261105/result.json`).
