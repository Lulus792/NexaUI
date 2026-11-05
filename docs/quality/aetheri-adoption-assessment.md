# Aetheri adoption: initial source assessment

## Deferred private-host output retirement (2026-11-05)

On candidate base `c3dc49068f54a969edf78473341591fabf4894c6`, the test-only
private layer host no longer aborts on a registry removal. It tracks its three
initial output global names and defers known output or layer-close retirement
until dispatch completes. Retirement cancels pending pointer release and
activation for that output, clears its focus, IME surface and accumulated
pointer deltas, then destroys its accessibility host, presenter, model view,
layer, surface and output proxy. Other views remain available. Unknown removed
globals do not destroy a view. Test-control commands targeting a removed first
output are ignored rather than dereferencing it.

The existing independent libatspi multi-view fixture now drives this same
retirement function: its old German search accessible becomes defunct and the
first view still accepts a query and Terminal intent. Callback tests check
focus removal and cancellation of pointer/activation state. A controlled
Wayland server test uses a real surface and a pending pointer-release sync;
retirement before its reply prevents activation. All three tests passed in
GCC static/shared and Clang ASan/UBSan builds with leak detection on Alpine.
A test-only mutant that did not schedule registry removal failed the wire
test's pending-removal assertion; the restored static test passed. The static
SDK was declared at `71b71fa9b7065ec9e49e9afeea69374e8fe0c8ac`; its core
sources are unchanged by this example-only candidate. The borrowed Aetheri
desktop model files have SHA-256 `eead8bbb85ff528b5e42500a288c567d72615de434cf20b33577ab5741ac9e95`
and `fe1bbb4061afa046463ca9190708f93940d4293649baaae3c1eda5960dbd9d10`.

Removal callbacks in these tests are simulated: no Aetheri compositor or real
output was removed. The fixture still requires three initial outputs and
cannot add a replacement output after removal. Native hotplug ordering, full
device-loss behavior, actual Aetheri shell ownership and human/physical
acceptance remain open; this does not complete D05 or W05.

## Labeled nested dialog with independent AT-SPI (2026-11-05)

On the candidate based on `504943f7be51d9b5d0d4ec869f2ccc4162d04e6f`,
the native nested-dialog fixture shapes and paints its four control labels with
the selected example font, and rejects frames without glyph bitmap commands.
It publishes the same frame's semantics. In a private Weston/D-Bus session,
the original virtual-keyboard flow now runs alongside an independent libatspi
client: the client finds the dialog trigger enabled and focused, observes it
disabled while the first menu action is enabled and focused, and sees the
trigger enabled and focused again after Escape. The action itself remains
keyboard-driven and is verified by the host's counter.

Both keyboard-only and keyboard-plus-AT-SPI cases passed using DejaVu Sans in
GCC static and shared Debug and Clang ASan/UBSan Debug with leak detection
(two cases per profile). An intentional probe expecting the covered trigger to
be enabled failed at the menu state check; the result is
`/tmp/nexaui-nested-accessible-negative-20261105-a/result.json`. The static
tests were rerun after restoring the probe and enabling compiler warnings.
The registered tests require a discoverable example font; the AT-SPI case
additionally requires the libatspi and private D-Bus tools.

These checks establish glyph command emission and native semantic states, not
human inspection of pixels, a screenreader task, physical input, other
compositors or actual Aetheri adoption. No SDK API or implementation changed;
L10 and A02 remain open for their full acceptance.

## Native keyboard for a nested modal and menu (2026-11-05)

On the candidate based on `9228a2306f46c91bc7eb97dbdfb0593ba8c709ee`,
a visible test-only Wayland software host exercises the existing modal and
popup APIs. On private Weston 14/Xvfb, a virtual keyboard focuses a trigger,
opens the modal and its menu, tabs to a second menu action and activates it.
The first Escape returns focus to the modal trigger; the second returns it to
the original trigger. The host checks the actual focus IDs and action count.

The native driver and existing headless overlay regression passed in GCC static
and shared Debug and Clang ASan/UBSan Debug with leak detection on Alpine/WSL.
An intentional negative control disabling Escape on the inner menu failed at
the expected modal-focus assertion; its original fixture result is
`/tmp/nexaui-nested-keyboard-mutant-20261105-a/result.json`. The host paints
control geometry but does not draw label glyphs or publish native semantics.
Earlier independent AT-SPI/UIA/AX checks cover other selected actions but do
not by themselves prove this exact visible keyboard scenario.

No SDK API or implementation changed. Full rendered labels and visual review,
human screenreader and physical-input acceptance, other compositors and actual
Aetheri integration remain open; L10 is not complete.

## Numeric field with Weston simple-IME (2026-11-05)

On the candidate based on `b2a29e28823478789693b896f87c9f89b52be988`,
the visible numeric test host opts into the existing borrowed-window IME API.
An independently running Weston 14 `weston-simple-im` supplies apostrophe
preedit and commits `á`; the draft becomes invalid while the committed numeric
value remains 1234. The driver verifies both the model and native v1 wire
events. The host now redraws only when routed events, geometry or a pending
presentation require it; the first continuously redrawing attempt did not
observe preedit and is not counted.

With this provider, Ctrl+Z then produced a native `commit_string` containing
`z`, yielding `1234áz` rather than undoing the draft. This happened in GCC
static and shared Debug and Clang ASan/UBSan Debug with leak detection; the
test explicitly reports that outcome and would also accept a correctly routed
undo. The earlier strict undo run failed and is preserved in
`/tmp/nexaui-numeric-real-ime-20261105-b/simple-ime.xml`. The four prior
Settings simple-IME cases and numeric independent-peer clipboard regression
also passed after the host change. No SDK API or behavior changed.

This is one virtual Weston simple-IME and keyboard map, not evidence that all
IMEs consume the shortcut. Undo with this provider, other installed IMEs,
physical input, human accessibility and actual Aetheri acceptance remain open.

## Numeric clipboard with independent Wayland peers (2026-11-05)

On the candidate based on `70380daf3e73ffdaa89a5ab34434236ca54d8b24`, a
visible test-only numeric host uses the public software presenter and optional
numeric clipboard callbacks. Under private Weston 14/Xvfb, virtual keyboard
input copies `1234` to an independent `wl-paste` process. Separate `wl-copy`
owners then supply `bad` and `42`: the host observes the invalid draft without
changing its committed 1234, then the valid draft before and after Enter commits
42. Escape performs normal host cleanup. At this candidate the fixture used a
50 ms poll with continuous redraw and was not an idle-performance example.

The registered CTest case executed three consecutive times in GCC static
Debug. The same driver passed once each with GCC shared Debug and Clang
ASan/UBSan Debug with leak detection, on Alpine/WSL; the external peers were
wl-clipboard 2.2.1. A negative control omitting numeric clipboard setup failed
at the independent paste client. An initial complete run timed out on Alt+F4
after exercising the clipboard; the test host now handles Escape explicitly.
Those earlier runs do not count as passing full runs.

These virtual compositor and keyboard checks do not establish real Aetheri
clipboard peers, other compositors, physical input, installed IMEs or human
accessibility acceptance. No SDK API or implementation changed; T05 and I05
remain open for their broader requirements.

## Numeric clipboard selection reversion (2026-11-05)

On the candidate based on `4ed4db364203c8ca5a82ce25a9621f565c7ded64`,
the controlled Wayland numeric clipboard fixture starts a delayed paste, moves
the selection, restores the original selection, and only then completes the
transfer. The draft and committed number remain unchanged. An intentional
negative control leaving the original selection untouched failed at that same
assertion, confirming the peer's bytes would otherwise be applied. The normal
and numeric asynchronous clipboard tests passed in static GCC, shared GCC and
Clang ASan/UBSan with leak detection (two cases per profile). The widget guide
also now describes the existing optional numeric IME and clipboard APIs rather
than claiming numeric clipboard is unavailable. No SDK implementation changed.

The peer is a controlled Wayland server, not an independent desktop clipboard
client. Actual Aetheri peers, installed IMEs, physical input and human assistive
acceptance remain open; this result does not complete T05 or I05.

## Full Settings example: small-window text reflow (2026-11-05)

On the candidate based on `71b71fa9b7065ec9e49e9afeea69374e8fe0c8ac`,
the full Settings example shares the existing example-only grapheme wrapper
with the original-model consumer. Long German labels at 128x150 retain their
requested font sizes rather than shrinking to fit; measured title, control and
status heights determine the scrollable layout. The public SDK is unchanged.
The copied installed example now includes the shared helper explicitly.

The scene regression passed with DejaVu Sans, Serif and Sans Mono. The scene
and three controlled text-input-v3 tests passed in static GCC, shared GCC and
Clang ASan/UBSan builds. A private Weston virtual keyboard exercised focus,
status paging, apply and clean close at the minimum size. The previous native
diagnostic used the former fixed 70-pixel viewport top and initially failed;
it now checks complete semantic bounds against the actual scroll viewport.
An independently copied software consumer built against the installed static
SDK declared at the base revision passed native keyboard and independent
AT-SPI apply/retry. Four private Weston simple-IME cases passed, including
composition, both focus cancellations and selection replacement. A preliminary
IME run lacked a built focus client and two cases failed; it is not counted.

These are automated virtual fixtures, not human screenreader, physical input,
200% text-scale, other fonts/scripts or actual Aetheri integration acceptance.
The installed SDK contains unchanged core sources from the stated base; the
example changes were the working candidate, not that committed revision.

## Bounded ASCII emergency wrapping (2026-11-05)

On the candidate based on `c075b03a8405c11898cc7cbb081c61260e768884`, the
Settings example's emergency wrapper can derive breaks for one unbroken
printable ASCII run from its existing shaped caret positions. It requires one
line, an ordered caret per byte and monotonic finite positions. The result is
reshaped and its maximum line width checked; failures and all other text use
the prior grapheme-by-grapheme path. Accessibility names remain the original
bytes. The helper is internal to the example; no SDK API or dependency changed.

The new direct test checks exact byte preservation, grapheme-boundary breaks,
measured width and an impossible-width failure over ASCII, ligature candidates,
spaces, combining sequences and Hebrew. DejaVu Sans, Serif and Sans Mono passed
on static, shared and Clang ASan/UBSan consumer builds (nine runs), with one
full Settings scene regression per build. A deliberately corrupted ASCII byte
copy failed the direct test at the expected byte comparison. An initial test
incorrectly assumed that every long string required inserted hard breaks;
the combining input could already wrap naturally, so that assertion was corrected.
The private Sway virtual-pointer and AT-SPI resize workflows passed on all
three builds (six runs). These are automated fixtures, not human reading or
physical input.

The same optional 16-display benchmark, DejaVu Sans, 120-byte unbroken ASCII
names, German 200% and 128x150, was run three times per source on Alpine/WSL2
with GCC 15.2.0, a Release consumer and the same installed static Debug SDK.
The exact base's cold frame took 294–337 ms and subsequent frames 18–27 ms;
the candidate's cold frame took 133–153 ms and subsequent frames 18–21 ms.
This wall-clock frame-construction experiment excludes native presentation;
host load, other text and other machines may differ. The selected display's
name includes a suffix and takes the general path. No target performance
budget, complex-script improvement, real Aetheri adoption or physical
responsiveness is established.

## Cached original-model Settings text layouts (2026-11-05)

The consumer candidate based on `d74583a03cb497b21cb616a421b3d848f8e8fd05`
retains at most 15 label and 16 display layouts, keyed by original text,
logical width, text size and small-layout mode. Unused display slots are
released; new layouts are created before their old slot is destroyed. Tests
exercise the same display ID with changed content, width and scale, alongside
the existing locale, selection, click, clipping and long-grapheme cases. A
mutant that ignored the width key failed the new displayed-height assertion.
Static, shared and Clang ASan/UBSan consumers passed those tests with DejaVu
Sans, Serif and Sans Mono (nine runs). The private Sway virtual-pointer and
independent AT-SPI resize workflows also passed for all three builds (six runs).

The optional `aetheri_model_view_benchmark` used 16 synthetic display names of
120 unbroken ASCII bytes, German 200% text and a 128x150 logical frame. On
Alpine/WSL2, GCC 15.2.0, a Release consumer and the same installed static Debug
SDK, three separate baseline runs from the base and three candidate runs each
measured one cold and five repeated frame constructions. Baseline cold ranged
339–427 ms and repeated frames 293–505 ms; candidate cold ranged 352–390 ms
and repeated frames 20–33 ms. These wall-clock observations include layout,
semantic and command construction, not raster presentation; host load varied.
The synthetic worst case is not an approved target or proof of physical-device
responsiveness. Cold layout, other fonts/targets, memory budgets, native input
latency and actual Aetheri adoption remain open.

## Pointer interaction on small Settings labels (2026-11-05)

The consumer candidate based on `5114c345c7833b303087b7d3754b0e63f5c23b5f`
registers clipped, non-focusable hit regions for the separately painted radio
and display labels at small sizes. Pointer presses transfer focus to the
existing control, and label clicks update its original model. Radios are built
after label input is resolved so checked semantics and marks agree in the same
frame; stale display labels are disabled. The ordinary controls and tab order
are retained, and no new accessibility nodes are added.

Controlled tests select labels in both directions and check same-frame checked
states, focus, cancellation when released outside, a display becoming stale
after input is queued, and no later replay. These scene tests passed with three
DejaVu fonts in static, shared and Clang ASan/UBSan consumers (nine runs).
On private Sway at 128x150, a virtual pointer clicked the Light label, after
which the existing keyboard resize/close flow succeeded for all three builds.
An old `5114c345c7833b303087b7d3754b0e63f5c23b5f` view, with only the focused
rectangle diagnostic backported to its host, failed the same native check:
the Wayland trace shows pointer enter at (24,130), button press and release,
and the original model remained unchanged. A separate AT-SPI resize regression
passed with the candidate's static consumer. Real physical input, native display
label clicking, screenreader review and actual Aetheri acceptance remain open.

The first virtual-pointer setup used three short-lived wlrctl devices; the
target saw pointer enter but the click device disappeared before the target
bound it, so those failures do not count as product evidence. The passing test
uses one guarded test helper retaining its device across motion and click;
the `wlr-protocols` 0_git20250702-r0 package was privately extracted and its
SHA-256 was `6017705dfd18698ea4b24c1adce8541747eb002f59307758b752f3804a2f205f`.

## Original-model Settings at 128x150 (2026-11-05)

The consumer candidate based on `e71ee304f1e6c23fa562d9e4b11f7e8e12152079`
uses a fully scrollable small layout below 320x240. Radio marks and display
buttons have separate wrapped labels; Page Up/Down can reveal labels taller
than the viewport. When an unbreakable word exceeds the available width, the
example inserts visible line breaks at existing grapheme boundaries, retaining
the original bytes in the semantic name. The minimum rendered size is now
128x150; smaller windows still do not receive a new frame. A selected mark's
click target is its row, not the separately drawn text below it.

Controlled scene tests cover German at 200% through 128/160/240/319x150,
keyboard focus visibility, paging, semantic selection of a display, and an
unbroken display name containing 40 combining-character graphemes whose
accessible name retains the original bytes. Static, shared and Clang
ASan/UBSan consumers each passed with DejaVu Sans, Serif and Sans Mono (nine
runs). Private Sway virtual-keyboard and independent AT-SPI resize workflows
passed at 128x150 for each of those three builds (six runs). The pre-change
`e71ee304f1e6c23fa562d9e4b11f7e8e12152079` host with the same installed SDK
timed out waiting for its 128x150 frame. A 128x150 high-contrast headless
image was inspected; it showed partial long text before the emergency wrap
and the split label afterwards. Human readability, other fonts/IME/layouts,
physical input, the cost of many long labels, and Aetheri adoption remain open.

## Virtual keyboard across a native Settings resize (2026-11-05)

On the consumer based on `7ab4c7fc4fb83801a38e67e1adade1695766d847`, a
private headless Sway/Pixman test used wtype 0.4-r0 to send virtual-keyboard
events. At 320x240 and default 100% English text, Tab moved focus through the
first two radio controls and Space changed the original model to Light. After
restoring 800x480, the diagnostic frame retained focus on Light; Tab and Space
selected High contrast, and Escape closed the process successfully. Static,
shared and Clang ASan/UBSan installed consumers all passed (three runs,
exit 0). The existing AT-SPI resize mode also passed again with the shared
consumer. An old host/view from `84ec8384ef30f36f3c2e2945e46e88d52ea46d63`
with the frame diagnostic added failed before keyboard input because it never
submitted a 320x240 frame; this does not isolate keyboard handling.

The first keyboard attempt without an explicit input delay reached and changed
Light but timed out on the first Tab after restore; its log showed focus still
on Light. The cause was not established. The passing driver invokes wtype with
a 100 ms sleep before each key and waits for observed frames/state rather than
extending test deadlines. The wtype Alpine package was extracted privately, not
installed system-wide; its package SHA-256 is
`e3237925ad960fd8e8aaa38bc9b0eb3033c1802a6e62be5a040747b13d02833e`.
This does not establish physical keyboard, other layouts, active IME, real
screenreader, physical monitor or actual Aetheri acceptance.

## AT-SPI actions across a native Settings resize (2026-11-05)

On the consumer and SDK based on `6c7253db0186e59e938552460a3fad42acd08c7c`,
the independent libatspi client selects high contrast, German and 200% text,
then pauses while private headless Sway resizes the actual software-rendered
window from 800x480 to 320x240. It observes the semantic window's new size,
checks the previous selections and changes text size to 150%. After restore to
800x480 it observes that size and the retained selections, chooses the second
synthetic display and closes via AT-SPI. The host's submitted frame/model logs
and Sway geometry are checked independently. Installed static/shared and
Clang ASan/UBSan consumers passed on Alpine/WSL2 with private Sway/Pixman and
a private accessibility bus (three runs, exit 0).

As a negative control, the same test and SDK used the original Settings view
and host from `84ec8384ef30f36f3c2e2945e46e88d52ea46d63`, with only the
current frame-size diagnostic added to the old host. Sway reported 320x240,
but the old host did not submit the required small frame; the client timed out
waiting for the resize handoff and exited 1. This exercises the old 480x380
cutoff, not the separately tested same-size configure wake. Native keyboard
interaction during resize, real input methods, human screenreader review,
physical displays and actual Aetheri adoption remain open.

## Same-size Wayland configure and Settings resize (2026-11-05)

On the candidate based on `993e02f1e352b3e38df855c43070ea70b7f55e2f`, an
unchanged-size Wayland configure queues `NX_EVENT_WAKE` while a visible window
has an external renderer. This lets an event-driven owner submit the surface
commit associated with the configure. Size-changing configures already emit a
resize event; the adapter's own background still commits without this wake.
The controlled protocol test failed on the base at `wakes>0`, then passed with
the candidate, and checks that releasing the renderer removes the extra wake.
Eight fractional-protocol cases plus that management test passed 9/9 in each
static, shared and Clang ASan/UBSan Wayland-only configuration. Initial broader
CTest selections included unbuilt binaries and are not counted as passes.

An installed C17 original-model consumer in each of those three configurations
ran under private headless Sway 1.12-rc3/Pixman as an unprivileged user. The
fixture waited for the tiled and floating frames, resized to 320x240, restored
800x480, checked each submitted frame and unchanged default model/display
fixture, then closed via SIGTERM (exit 0). Earlier attempts issued resize while
the floating transition was still settling and timed out; the sequenced fixture
passes on both old and new SDKs, so it does not establish that the wake fix
caused the resize result. It also does not test keyboard, AT-SPI or a real
Aetheri session during resize. Physical/human acceptance, other compositors,
Vulkan-specific presentation and windows below 320x240 remain open.

## Original-model Settings narrow view (2026-11-05)

On the consumer tree based on NexaUI
`84ec8384ef30f36f3c2e2945e46e88d52ea46d63`, the installed C17 example now
wraps radio labels and sizes their rows for the actual measured text. At narrow
widths the title is part of the scrollable content, leaving room to reveal
focused controls. The former host skipped new frames below 480x380; the new
limit is 320x240. Sizes below the new limit still do not receive a new frame.

The controlled scene test covers focus traversal and full focused-control
visibility inside the scroll viewport at widths 320, 360, 399, 400, 440 and
480 with height 240, German and 200% text. Installed static, shared and
Clang ASan/UBSan SDK consumers each passed with DejaVu Sans, Serif and Sans
Mono (nine runs, exit 0). The pre-change scene rejected 320x240. A headless
320x240 frame was visually inspected; an initial frame exposed text/mark
overlap, and the corrected frame was inspected again. This is not a native
resize, hardware scaling, full contrast or human accessibility acceptance.
The existing independent AT-SPI client also passed on private Weston at the
host's normal initial size, changing the appearance, language, text size and
fixture display before closing. That regression does not test a native resize.
The external Aetheri model snapshot's model.c/model.h SHA-256 values are
`3ee6f265faa83b884d387b363e77ac327a69834b2d1aac250ac557a5007b437c` and
`776de312e1cfed97e972c09fb8a56cca0b94748d4e410e5b8278b0335a12e793`,
matching the unmodified model files in Aetheri revision
`6891615dc9803e3a0d328534f17731a130645d4b`. No Aetheri files or pins changed.

## Native two-axis scrollbar actions (2026-11-05)

The added C17 test host and independent libatspi client, on the source tree
based on `93384d478c27b54a1940023546c1f686035f7ef6`, exercise horizontal and
vertical scrollbar increment and decrement actions in one area. The client
observes 0/0, 10/0, 10/10, 0/10 and 0/0 percent before closing the host via
AT-SPI; the host reports its final application-owned offsets. On Alpine/WSL2,
private Weston 14/Xvfb and an unprivileged test process, direct fixture runs
passed for static, shared and Clang ASan/UBSan builds (exit 0 each); the
registered CTest also passed 1/1 in the static build. An older locally installed
static SDK failed the same client at the first horizontal step, observing 0/0
instead of 10/0. The initial CTest attempt failed before running the test
because its existing log directory was not writable by the unprivileged user;
the later run used a writable test-log directory. This is automated private
compositor evidence, not physical input, a human screenreader, other platforms
or Aetheri adoption. The fixture is `nexaui_atspi_scroll_wayland_private` and
requires the existing private Weston wrapper and AT-SPI test dependencies.

## Borrowed host scale and presenter ownership (2026-11-05)

Test source `920787e36bfa298e734be5fc644cfb7cd20e0d2d` exercises the existing
borrowed Wayland presenter with a controlled private role while three buffers
are held: 40x20 at 1x, 80x40 at 2x, then 40x20 at 1x. Server-side assertions
check dimensions, transparent and translucent pixels and scaling of a drawn
rectangle before the held buffers are released. On Alpine/WSL2, Clang 22.1.3,
static Wayland-only Debug with ASan/UBSan and leak detection, this test passed
five consecutive runs. This is a protocol fixture, not real output hotplug,
physical scaling or Aetheri's private role.

The archived Aetheri desktop source `d621c9bba1df8ffd1083999300bcff742022b9dc`
creates SDL custom-role Wayland windows, obtains their surfaces and display,
assigns layer-shell roles and then creates SDL GLES renderers for those windows.
NexaUI's borrowed presenter requires sole ownership of buffer presentation on
its surface. A host integration must explicitly retire or replace the SDL
presenter for each adopted surface; attaching both renderers concurrently is
not established by this test. Role, input, scale, app lifecycle and semantic
ownership remain host obligations. No Aetheri checkout or consumer pin changed.

## Sibling overlay stack and keyboard checks (2026-11-05)

Implementation source `e1c23d95c3affb1a412c60ae9b51f1666c51425a` extends
the bounded overlay stack to sibling scopes while retaining nested scopes.
Test source `5a5c74708d24070a9c4b1d87eca566e602e04420` covers ordering,
keyboard focus cycling, pointer blocking, Escape/outside dismissal, focus return,
removed/reordered scopes, allocation failure and the 16-scope limit. The
Windows shared Debug overlay, search and independent UIA tests passed on
Windows 10.0.26200 with MSVC 19.38; the core overlay/search tests also passed
on Alpine/WSL with Clang 22.1.3 ASan/UBSan and leak detection. See the next
section for distinct native client sources and their test boundaries.

The six core CI jobs for earlier equivalent implementation source
`c8cbc8bc41f770b6cdda33046e10df02700ce526` passed Linux, Windows and macOS
static/shared ([run](https://github.com/Lulus792/NexaUI/actions/runs/42024215210));
its Linux sanitizer job was still running at this inspection and Windows ASan
was skipped. A local Windows ASan Release attempt timed out on both core tests
and is not counted as a pass. The intervening changes only add tests and
documentation. Physical/human assistive and Aetheri acceptance remain open.

## Sibling overlay candidate: native checks (2026-11-05)

Source `a4cebc1c1e98b2690049b7969eed851cb268801e` extends the independent
macOS AX client to observe and activate sibling popups. The external client
passed on both variants; the static job completed 112/112, while the shared
job failed its separate `nexaui_nexacloud_metal_native_example` after the AX
test passed ([run](https://github.com/Lulus792/NexaUI/actions/runs/42023734697)).
The Metal failure's cause was not established. Its log reports reference-equal
frames and timing data; it is not a successful complete shared job.
The later candidate `c8cbc8bc41f770b6cdda33046e10df02700ce526`, which adds
profile-phase diagnostics without changing acceptance checks, passed both
complete macOS jobs (111/111 shared, 112/112 static), including AX and Metal
([run](https://github.com/Lulus792/NexaUI/actions/runs/42024215210)). A later
pass does not establish the cause of the earlier failure.

The Windows UIA test at `63baf5e1f93e8d3bafa5b6a7e9c444e36bc108df` passed
three runs each in STA, MTA and owner-initialized COM on Windows 10.0.26200,
MSVC 19.38, shared Debug. A control with old product source `c8c763b` rejected
the second popup. The Wayland AT-SPI test at `7682eed2a51f3651c04732934ff686d67470e299`
passed on private Weston/Xvfb, while the same test with `c8c763b` failed when
opening the second popup. The first Weston setup lacked helper paths and failed;
only the corrected run passed. These client checks do not establish human
screenreader or actual Aetheri acceptance.

## Nested overlay external macOS client candidate (2026-11-05)

Test source `b2851034be5a9398d10626d921428777e76b9a4f` extends the independent
macOS AX client and host with a nested-overlay interaction and host-observed
action count and logical focus return. At that inspection the native CI result
was pending; the completed result is recorded below. As a separate core
regression on that source, Windows 10.0.26200 x64 with MSVC 19.38.33145 shared
Debug passed `nexaui_overlay` and `nexaui_search` five consecutive times each.
Those Windows tests are not macOS AX, VoiceOver or Aetheri acceptance.

## Independent macOS AX nested overlay client (2026-11-05)

Test source `fdbf7242f851e2a3c9a6af94f2ddabc15bfb7408` adds an independent
AX client check for a nested modal and popup. It checks the disabled parent,
enabled inner action and genuinely disabled child, invokes the inner action,
and observes the host's action count and logical focus return. The equivalent
test tree `b2851034be5a9398d10626d921428777e76b9a4f` passed the external test
and full macOS shared (111/111) and static (112/112) CI suites
([run](https://github.com/Lulus792/NexaUI/actions/runs/42020716816)).
The additional intervening main changes only modify the Windows IME test and
documentation. This is an automated client result, not human VoiceOver,
physical or Aetheri acceptance. Sibling overlays remain a separate open item.

## Windows IME focus transfer (2026-11-05)

Test source `e4ce4d1f554f5c4fc3eee6fbe5e41869a7d7cf49` adds a controlled
same-window transition between two numeric fields: an old composition result
must not reach the newly focused field. On Windows 10.0.26200 x64, MSVC
19.38.33145 shared Debug, the new test passed ten consecutive runs on the
equivalent test tree based on `b2851034be5a9398d10626d921428777e76b9a4f`;
the only additional committed differences there were macOS test files and
documentation. An isolated build with target-change revocation deliberately
disabled failed exactly at the new no-text-event assertion. These are injected
messages, not acceptance of a physical keyboard or installed IME. T05 remains
open and no Aetheri consumer pin changed.

## macOS selection notification comparison (2026-11-05)

The native macOS static and shared CI jobs for source
`8da965030f62b990ceac3e1d7aea120ca3a52a64` completed successfully
([run](https://github.com/Lulus792/NexaUI/actions/runs/42015529187)).
After the additional prefix-change scenario, the static job for test-only
source `430fcfb8fdfbf13a6f0f5ba58098cb2512c1619c` failed in the independent
AX client at line 131, the assertion after changing the prefix while retaining
the native selection ([static job](https://github.com/Lulus792/NexaUI/actions/runs/42017235646/job/127210696808),
[shared job](https://github.com/Lulus792/NexaUI/actions/runs/42017235646/job/127210696722)).
The earlier invalid-selection assertion is at line 126 and passed in both jobs.
The counterexample exercises the added prefix case; it does not establish that
the subsequent implementation fixes it.
The subsequent implementation `1bb0c11c4401b0f248115d0346be4e3f4f21f2fd`
passed the independent AX client and the complete native macOS test sets:
111/111 shared and 112/112 static
([run](https://github.com/Lulus792/NexaUI/actions/runs/42017296511)). The
same run's Linux and Windows core jobs and Linux sanitizers also passed; the
Windows ASan and fuzz jobs were skipped. A later merge with Windows IME changes
is a separate CI candidate ([run](https://github.com/Lulus792/NexaUI/actions/runs/42019931482)).
These automated client checks do not replace VoiceOver, physical IME or Aetheri
acceptance.

## Windows numeric IME result identity (2026-11-05)

Source `c0aeaada667f3e6644851636ec795f6e650e7486` associates a controlled
`WM_IME_CHAR` from an active self-drawn composition with its editor and revision.
The numeric field can retain an invalid IME result as an undoable draft without
committing it; regular `WM_CHAR` remains ordinary filtered input. A changed
editor/revision or lost focus invalidates the tracked composition. The native
surrounding-text API remains unsupported on Windows.

On Windows 10.0.26200 x64, MSVC 19.38.33145, the shared Debug IME and numeric
tests each passed ten consecutive runs, and the three UI Automation value test
variants each passed five runs. Static Debug IME and numeric tests each passed
five runs. The fixture covers a supplementary Unicode scalar, invalid draft,
undo, valid multi-character result, commit, stale model, focus loss and legacy
untargeted input. The same test with unchanged main source
`c131ea0ce62859984e160e388b6bc00aad620ac6` failed at the invalid-draft
assertion. These are injected Win32 messages, not evidence from an installed
IME or a human screenreader; ordering and cancellation across real IMEs and
Aetheri acceptance remain open. T05 is not complete.

## Controlled Windows numeric IME composition (2026-11-05)

Source `4ce0a36a231fab58338063f1e75f492f51337812` extends the existing
Win32 IME fixture with a numeric field: injected native preedit is painted
without changing its draft or committed value; focus loss clears preedit;
a controlled `WM_IME_CHAR` reaches the field once and a later Enter commits
the valid number. On Windows 10.0.26200, MSVC 19.38.33145, shared Debug,
the IME and numeric field tests passed ten consecutive repetitions each.
A deliberate test-only mutation that did not bind the numeric field to the
window failed its preedit-line assertion. Restoring the source initially left
the older mutant executable in place because its object was newer than the
restored file; that repeat failed and is not a pass. Touching the restored
source forced recompilation before the reported successful repetitions.
This exercises controlled Win32 messages and test preedit, not an installed
IME, physical keyboard, Windows screenreader or Aetheri acceptance; T05 stays
open. No Aetheri consumer pin was changed.

## macOS AX text selection and UTF-16 conversion (2026-11-05)

Source `6125a152c523afd3b4821637d9b6fafa664a0077` adds macOS AX selected-text,
selected-range and character-count queries and a bounded guarded selection
setter. Native UTF-16 ranges are converted to core UTF-8 byte offsets; invalid
surrogate boundaries and out-of-range values are rejected, and core grapheme
validation remains authoritative. Pending selection changes expire on a newer
published frame or an invalidated model. Protected fields expose no selection;
enabled read-only fields permit selection without permitting value changes.

The test-only `36651e6918c27027d98870e0cf510d18542ab1b1` failed before the
implementation in the native selector and independent AX client
([static](https://github.com/Lulus792/NexaUI/actions/runs/42014114805/job/127200185128),
[shared](https://github.com/Lulus792/NexaUI/actions/runs/42014114805/job/127200185178)).
The corrected source passed the complete macOS ARM64 suites
([static 112/112](https://github.com/Lulus792/NexaUI/actions/runs/42014683317/job/127202116569),
[shared 111/111](https://github.com/Lulus792/NexaUI/actions/runs/42014683317/job/127202117153)).
The independent client set and read an emoji selection and the selected text.
In-process cases additionally cover half-surrogate and overflow rejection,
stale model replacement, read-only and protected fields. Text-range editing,
geometry and selection-change notifications remain open, as do VoiceOver,
physical IME and Aetheri acceptance; this does not complete T05.

## Guarded macOS AX whole-value editing (2026-11-05)

Source `6491d77caef0587293ebb4f3dbad2e3039da572d` adds bounded whole-value
replacement for real writable text and numeric fields to the AppKit AX bridge.
Requests copy UTF-8 and retain a revocable lease, text identity/revision and
native incarnation through owner-thread routing and core application. Disabled,
read-only and protected targets cannot be edited; replacing a text model retires
its old native element. The native queue and payload limits remain bounded.

The preceding test-only `fc79fa4c0562466a7a00905e888211b569f82164` failed at
the expected unavailable setter in both
[static](https://github.com/Lulus792/NexaUI/actions/runs/42012142086/job/127193540560)
and [shared](https://github.com/Lulus792/NexaUI/actions/runs/42012142086/job/127193540419)
macOS jobs. The corrected source passed the complete native suites:
[static 112/112](https://github.com/Lulus792/NexaUI/actions/runs/42013208132/job/127197143765)
and [shared 111/111](https://github.com/Lulus792/NexaUI/actions/runs/42013208132/job/127197143493).
An independent ApplicationServices client successfully replaces numeric and
Unicode text values and observes publication. The in-process AppKit case also
checks invalid numeric drafts and commit, model replacement after routing,
disable/re-enable, stale retained elements, read-only/protected targets,
single-line newline rejection and the payload bound. This is whole-value
editing; the subsequent selection work above adds native text selection.
Text-range editing, physical IME, VoiceOver and Aetheri acceptance remain open,
and no Aetheri consumer pin changed.

## Numeric value visibility through macOS AX (2026-11-05)

Source `efea520b9f12d7e2bd34035164980e8aae4846d7` adds a real numeric field
to the existing AppKit AX fixture. The in-process test and the independent
ApplicationServices client observe its text-field role and current value and
verify that AX value replacement is unavailable. On native macOS ARM64 CI,
the complete suites passed: [static 112/112](https://github.com/Lulus792/NexaUI/actions/runs/42011616407/job/127191793640)
and [shared 111/111](https://github.com/Lulus792/NexaUI/actions/runs/42011616407/job/127191793708).
The external AX case was executed and passed in both, rather than skipped.
At that source revision it confirmed read-only visibility and the then-missing
editing capability. The later whole-value implementation above supersedes that
limitation; text ranges, VoiceOver, physical IME and Aetheri acceptance remain open.

## Numeric value through Windows UI Automation (2026-11-05)

Source `6dde095865b6465dd944ff702cc399f0f834c210` extends the existing
independent Windows UIA ValuePattern client and host with a numeric field.
The client reads the initial value, sends an invalid draft and observes the
native invalid-form property while the host checks that the committed number
remains unchanged. It then sends a valid draft, observes the valid state and
the host commits the expected value. The full UIA value regression passed in
STA, MTA and owner-initialized COM modes (3/3, exit 0) on Windows 10.0.26200,
MSVC shared Debug; the native window was hidden. This is an independent client,
not human screenreader or physical/visual acceptance. It does not establish
Windows native text-selection or IME composition behavior, macOS AX numeric
editing, or Aetheri consumer acceptance. T05 remains open.

## Accessible numeric text and selection (2026-11-05)

Source `a65206f0b740069ae4a3d5f4f0a9eb93c25501f7` publishes the numeric
field's current anchor and cursor with its existing text model. Before this
change, an independent AT-SPI client found the numeric node but could not obtain
its text interface: the Linux bridge requires an actual selection snapshot.
The new client case failed on that missing interface with the previous source
(`/tmp/nexaui-numeric-atspi-check/result.json`).

On Alpine/WSL private Weston, an independent libatspi client now sets an invalid
draft, reads it and observes the invalid state while the host confirms its
committed value is unchanged; it then sets a valid draft, manipulates selection
and caret and asks the host to commit it. This ran as part of the complete
existing large-text native test in GCC shared Release and Clang ASan/UBSan
with leak detection (1/1 each; fixtures `/tmp/nexaui-numeric-atspi-check3` and
`/tmp/nexaui-numeric-atspi-asan-final`). Numeric, semantic, numeric clipboard,
and v1/v3 input regressions passed in the sanitizer build (7/7). The Windows
MSVC shared Debug numeric field test passed (1/1); an initial request for a
Linux-only semantic test target failed before that separately executed check.
The source applies across platforms, but native Windows UIA and macOS AX numeric
editing, Aetheri integration and human screenreader acceptance remain open.
T05 is not fully accepted; no consumer pin was changed.

## Deferred fixture disconnect after request dispatch (2026-11-05)

On `1b5699f906979fe8aa5cdfdbecebb7c0a896f672`, the Ubuntu debugger caught
the multi-window test crashing inside `libwayland-server`'s event dispatch,
while the main test thread waited for the fixture server to stop: see the
[static job](https://github.com/Lulus792/NexaUI/actions/runs/42008136380/job/127180001922)
and [shared job](https://github.com/Lulus792/NexaUI/actions/runs/42008136380/job/127180001771).
Both had reproduced a SIGSEGV in the test suite; debugger retries caught the
multi-window crash on their second and first attempts, respectively. This
places the failure in the fixture server but does not alone prove the cause.

The fixture had destroyed a Wayland client directly inside two request
handlers. The Wayland server implementation continues to use the client and
connection after dispatching a request (see the
[Wayland server source](https://chromium.googlesource.com/external/wayland/wayland/+/refs/heads/master/src/wayland-server.c)).
Source `d8b9db9d22656c7f1a70bd8f8639a3b37c533159` instead schedules the
simulated disconnect on the server loop's idle phase, after request dispatch.
The same five client-facing failure scenarios still execute. Alpine/WSL GCC
15.2 shared Release passed 30 repetitions each; Clang ASan/UBSan with leak
detection passed ten each (`/tmp/nexaui-deferred-disconnect-release.log`,
`/tmp/nexaui-deferred-disconnect-asan.log`). Neither local pass alone proves
the Ubuntu failures are fixed. This change only touches the controlled test
fixture, not the NexaUI client.
Ubuntu CI for `d8b9db9d22656c7f1a70bd8f8639a3b37c533159` subsequently passed
the complete native suites: [static 239/239](https://github.com/Lulus792/NexaUI/actions/runs/42009404549/job/127184266805)
and [shared 237/237](https://github.com/Lulus792/NexaUI/actions/runs/42009404549/job/127184266756),
including the previously failing multi-window, disconnect and Settings reopen
cases. Both jobs also completed their installed-consumer checks. Windows and
macOS core jobs in the same run passed; the separate Linux sanitizer job was
still running when this evidence was recorded. This supports the fixture fix
on Ubuntu without claiming the entire adoption or physical-device acceptance.
The wider local Wayland fixture regression on `a346b47ca7b017927bc9a228c255a06cd02132f4`
also passed 47/47 cases (GCC Release shared, exit 0), including scaling,
software, host, IME, clipboard, drag/drop and multi-window checks; command
output is `/tmp/nexaui-fixture-all-test.log`. This is still Alpine/WSL evidence.

## Numeric input through controlled Wayland text-input-v1 (2026-11-05)

Source `a01cbee4d45a3726b373564369c8b46eabc244e9` extends the existing
controlled native v1 test with a numeric field. The fixture sends a relative
deletion and invalid commit; the test verifies the resulting invalid draft,
unchanged committed value and rejection of a replayed stale batch. On
Alpine/WSL, GCC 15.2 Release shared Wayland-only, v1 and v3 each passed ten
consecutive runs (ctest `--repeat until-fail:10`, exit 0). This is controlled
protocol evidence, not an installed IME, other platform, Aetheri, or human
acceptance; T05 remains open. On current source
`1b5699f906979fe8aa5cdfdbecebb7c0a896f672`, both tests also passed in the
Alpine/WSL Clang AddressSanitizer/UndefinedBehaviorSanitizer build with leak
detection enabled (2/2, exit 0). The build emitted three existing missing-field
initializer warnings in `src/nexaui.c`; no sanitizer failure was reported.

## Settings reopen test output isolation (2026-11-05)

On Ubuntu CI, source `3d1c27fbd85e6fae952df223c7a97f6180176f1a`
identified the failing reopen test's unexpected first marker as a Vulkan
validation-layer warning on stdout ([shared job](https://github.com/Lulus792/NexaUI/actions/runs/42006206022/job/127173472897)).
The test previously redirected the entire process stdout into its strict
marker protocol. This explains the marker mismatch, not the separate Wayland
disconnect segfaults.

Source `b1a18b3b523ad0b039a9c4237e993e70d62367ba` passes an inherited test-only
descriptor for the two reopen markers while leaving stdout available to the
application and libraries. Without that descriptor the example retains its
stdout marker behavior. The independent client emits unrelated stdout before
starting Settings: on the previous code the private Weston test failed at the
expected marker; after the change, the reopen and unknown-outcome tests both
passed on Alpine/WSL with Vulkan validation enabled (2/2, exit 0; fixture
`/tmp/nexaui-reopen-noise-after/result.json`). This checks channel isolation,
not the Ubuntu CI result or broader Settings acceptance. The Ubuntu jobs and
their independent disconnect stack diagnostics remain outstanding. The later
`6270109` Ubuntu CI passed the reopen case in both
[static](https://github.com/Lulus792/NexaUI/actions/runs/42007186198/job/127176799982)
and [shared](https://github.com/Lulus792/NexaUI/actions/runs/42007186198/job/127176799892)
builds; each suite still failed the separate multi-window disconnect case.

On the subsequent `7fc4742` Ubuntu CI, the disconnect tests again segfaulted
in the suite, but the single debugger rerun exited normally in both
[static](https://github.com/Lulus792/NexaUI/actions/runs/42006411419/job/127174159715)
and [shared](https://github.com/Lulus792/NexaUI/actions/runs/42006411419/job/127174159993)
jobs. This gives no stack or cause. The rerun did not reproduce all suite
environment variables, and debugger timing could also matter. The diagnostic
now repeats with the suite environment; the cause remains unproven.

## Nested overlays through macOS AX and CI portability (2026-11-05)

Source `de808bdacef1e35111a6f0c69338f6017afab153`, based on the test-only
`a866fd52b6cd4a49fceb0f8f3d370fbe7d022676`, separates intrinsic disability
from overlay suppression in the macOS AX snapshot, retaining inherited
offscreen state. The in-process native AppKit test checks the disabled parent,
active inner action and its actual delivery, restored logical focus and a truly
disabled group's child. The preceding test-only candidate failed exactly at
the inner action on both native macOS configurations
([static job](https://github.com/Lulus792/NexaUI/actions/runs/42003819088/job/127165441373),
[shared job](https://github.com/Lulus792/NexaUI/actions/runs/42003819088/job/127165441163)).
The corrected source passed the complete macOS CI suites: 112/112 static and
111/111 shared on macOS 26.7.2 ARM64, image `macos-26-arm64/20261102.0473.1`
([static job](https://github.com/Lulus792/NexaUI/actions/runs/42004372290/job/127167303385),
[shared job](https://github.com/Lulus792/NexaUI/actions/runs/42004372290/job/127167303569)).
The pre-existing independent AX client also passed as a regression, but the
new nested action is exercised in-process; an independent nested AX workflow
and human VoiceOver acceptance remain open.

The same source corrects three evidenced CI portability defects. The Linux
clipboard-send fixture declares the glibc feature needed for `pipe2`; the
numeric test no longer assumes macOS Home moves the caret; the Settings scene
declares its own Linux math dependency. In the Windows shared Debug build,
the Settings test previously timed out at 20 seconds because its executable
could not locate the core DLL. Setting the test working directory to the core
target directory passed in 0.82 seconds without increasing the deadline
(`D:/NexaUI/out/nested-settings-ci-debug-20261105/settings-scene-final.log`).
The prior macOS CI failed numeric-field assertion and Ubuntu glibc jobs failed
`pipe2` declaration or the Settings `fmaxf` link; the corrected macOS numeric
test passed in both jobs, and the corrected Ubuntu jobs passed their build step.
Local Alpine shared Settings, numeric and two native clipboard-send cases passed
(`/tmp/nexaui-portability-final.log`). Later CI steps may have independent
results and are not inferred from those build passes. The package version is
0.1.11; no Aetheri pin or 1.0.0 release was changed.

The full `de808bd` Ubuntu CI did **not** pass: both variants failed the new
X11 focus assertion because the private-Wayland fixture flag was inherited by
X11 tests, and the Settings AT-SPI reopen case failed in both. Static also
reported a disconnect-reentry segfault; shared reported multi-window-failure and
disconnect segfaults. See [static job](https://github.com/Lulus792/NexaUI/actions/runs/42004372290/job/127167303526)
and [shared job](https://github.com/Lulus792/NexaUI/actions/runs/42004372290/job/127167303446).
Follow-up `7afb1f7af8abaa10ccd48368fc5ae0b3225788e0` scopes the focus assertion
to actual Wayland; two independent X11 AT-SPI tests passed locally with the
private-Wayland environment flag present (`/tmp/nexaui-nested-x11-final.log`).
The reopen case passed once on local Alpine/WSL Weston with Vulkan validation
(`/tmp/nexaui-reopen-investigate.log`); 30 local repetitions of disconnect
reentry with ASan and of multi-window failure also passed
(`/tmp/nexaui-drop-reentry-repeat.log`, `/tmp/nexaui-multi-failure-repeat.log`).
Those local results do not explain or erase the Ubuntu failures. Their causes
and the follow-up CI run remain open.

## Nested overlays through Windows UI Automation (2026-11-05)

Source `fafd19e808475d46e8f44db71b2dd9d3ddaa0bfd` applies the same distinction
between intrinsic disabled state and overlay suppression to the Windows UIA
snapshot. Real disabled state and offscreen state still inherit through the
native tree. The change is in the 0.1.10 development package.

The existing independent UIA client and host now exercise a nested modal and
popup: an explicitly disabled group's child remains disabled; the outer trigger
is disabled while the inner action is enabled and can be invoked; the host
observes exactly one activation; closing the child restores the logical focus
to the trigger, which becomes enabled again. On the preceding `f354ade` source
with these new tests, the native client rejected the incorrectly disabled
inner action (`D:/NexaUI/out/nested-uia-20261105/before.log`).

On Windows version 10.0.26200, x64, MSVC 19.38.33145 and Windows SDK 10.0.26100,
the static Release build passed nine independent UIA tests across STA, MTA and
owner-initialized COM, including existing value and multi-selection regressions
(`D:/NexaUI/out/nested-uia-20261105/regression.log`). The shared Release build
with MSVC AddressSanitizer passed the nested UIA test
(`D:/NexaUI/out/nested-uia-asan-20261105/after.log`); its build warned that
debug information was not enabled, limiting sanitizer diagnostics. These test
windows are hidden: the focus-return assertion uses the host's logical focus,
not a visible desktop or human screenreader observation. Neither local build
embedded `NEXAUI_SOURCE_REVISION`; the source above names the tested tree, not
an in-binary provenance result.

macOS AX has analogous inherited-state logic and still requires correction and
native verification. Human screenreader, Aetheri and physical/visual acceptance,
as well as sibling overlays, remain open. No 1.0.0 release was made.

## Nested overlays through Linux AT-SPI (2026-11-05)

Source `01b04679211e380e28a6e341e1ca9f5df4967353` fixes a Linux bridge
regression in the 0.1.9 development package. The bridge previously inherited
an overlay-derived disabled state from the outer trigger into the active nested
popup. It now inherits actual local/ancestor disabled state separately while
retaining each node's overlay-derived state. A truly disabled group still
disables its child.

An independent libatspi client drives a separate NexaUI host on private Weston
14.0.2/Xvfb and a private D-Bus/AT-SPI registry. It opens a modal and nested
popup, observes the outer trigger disabled and inner action enabled and focused,
verifies the outer action is rejected, activates the inner action and observes
the host's count, closes the popup, and observes the trigger enabled and
focused again. The added disabled-group case verifies real disabled inheritance.
Before the bridge fix this client failed its enabled-state assertion; the
failed fixture result is `/tmp/nexaui-nested-atspi-20261105/result.json` and
the diagnostic is retained in the execution transcript.

On Alpine/WSL, GCC 15.2 Release, Wayland-only, AT-SPI-enabled shared build
passed that native case plus existing native multi-selection and large-text
cases (3/3, `/tmp/nexaui-nested-atspi-019.log`, compositor result
`/tmp/nexaui-nested-atspi-019-20261105/result.json`). Clang 22.1.3 Debug
ASan/UBSan with leak detection passed the independent nested case (1/1,
`/tmp/nexaui-nested-atspi-019-asan.log`). The overlay, search, semantics and
multi-selection core regressions passed under ASan/UBSan before the final
version-only bump (`/tmp/nexaui-nested-atspi-core.log`). Static/shared installed
C17 package checks also passed (`/tmp/nexaui-nested-atspi-019-package.log`).
Those local native builds did not embed a `NEXAUI_SOURCE_REVISION` value; the
source above identifies the tested tree rather than a linked provenance query.

This is an automated private-compositor/client result, not a human screenreader
or Aetheri acceptance. Windows UIA and macOS AX source use similar disabled-state
inheritance and still need correction and native tests. Sibling overlays remain
unsupported. No Aetheri consumer pin or 1.0.0 release was changed.

## Nested overlay focus scopes (2026-11-05)

Source `d929460b54ca47fe048b1a49784773f17afa8b6c` adds up to 16 nested
overlay scopes in the 0.1.8 development package. The innermost active scope
receives focus and dismissal; closing it restores focus to its enclosing scope.
The semantic parent may be the enclosing panel or one of its descendants.
Nested searchable selection reuses the existing widget and its failure rollback.
Applications must build inner scopes after enclosing content; sibling overlays
in one frame remain unsupported.

Headless tests cover an inner popup in a modal, Tab and Shift-Tab, Escape,
outside click, programmatic close, opening both levels together, omitted inner
views, accepted inner and rejected outer semantic actions, a nested searchable
select and activation, allocation failure/retry, invalid parent and the 16-level
limit. The pre-change `ad42920` failed the nested-open test
(`/tmp/nexaui-nested-before.log`). An isolated mutant that allowed Escape to
dismiss every layer failed the inner-only dismissal check
(`/tmp/nexaui-overlay-mutant-test.log`); it is not shipped.

On Alpine/WSL the overlay, menu, search, semantics, multiselection, numeric and
text tests passed in Clang 22.1.3 ASan/UBSan with leak detection (seven tests,
`/tmp/nexaui-nested-final-asan.log`). Overlay, menu, search, semantics and
multiselection passed in GCC 15.2 Release Wayland-only and GCC Debug dual
Wayland/X11 (`/tmp/nexaui-nested-final-release.log`,
`/tmp/nexaui-nested-final-dual.log`); the final overlay/search cases passed again
after the additional semantic action assertion (`/tmp/nexaui-nested-release-last.log`,
`/tmp/nexaui-nested-dual-last.log`) and under ASan/UBSan. Static and shared
installed C17 package checks passed (`/tmp/nexaui-nested-package.log`).
These are headless interaction/semantic tests, not native screenreader or
Aetheri visual acceptance; L10 remains open for those and sibling overlays.

## Numeric clipboard integration (2026-11-05)

Source `75d78e0a12d8262227d3518640423f627990e47b` adds the optional
`nx_numeric_field_set_clipboard` to the 0.1.7 development package. Numeric
fields reuse the general-field clipboard callback validation and bounded native
Wayland asynchronous reader. A complete paste is applied to the draft; invalid
text is visible and does not change the committed value. Model replacement,
focus changes and removing callbacks cancel or invalidate pending edits; field
destruction cancels its outstanding native transfer. The application must keep
callback data and any borrowed native window alive through that lifecycle.

The numeric widget test covers copy, cut, paste, invalid multiline text,
semantic invalid state and reentrant model replacement. A controlled Wayland
peer supplies an actual asynchronous paste; tests check confirmation, stalled
peer/model replacement, focus away and back, and destruction with the reader
slot subsequently reusable. Clang 22.1.3 ASan/UBSan with leak detection ran
eight selected tests, GCC 15.2 Release Wayland-only seven, and GCC Debug dual
Wayland/X11 four (`/tmp/nexaui-numclip-asan.log`,
`/tmp/nexaui-numclip-release.log`, `/tmp/nexaui-numclip-dual.log`). The final
native numeric case including confirmation passed separately in all three
profiles (`/tmp/nexaui-numclip-native-final.log`,
`/tmp/nexaui-numclip-release-final.log`, `/tmp/nexaui-numclip-dual-final.log`).
An isolated mutant that omitted cancellation on destruction failed the
reader-slot check (`/tmp/nexaui-numclip-mutant-test.log`); it is not shipped.
Static and shared installed C17 consumers linked and called the new symbol
with X11 discovery disabled (`/tmp/nexaui-numclip-package.log`).

The server is a controlled native Wayland fixture, not an independent Aetheri
peer or physical/human acceptance. Other native IME implementations, actual
Aetheri clipboard interactions and native assistive numeric editing remain open;
T05 is not fully accepted. No Aetheri consumer pin was changed.

## Reentrant text clipboard callbacks (2026-11-05)

Source `3ec8314e53804b94e9034954faf5be34f3f341c3` protects the shared
general-text clipboard path when a callback replaces the field model or changes
focus. A writer receives a separate snapshot of the selected bytes; a pending
cut or paste is applied only if model identity, generation, selection/buffer
serial and focus generation still match. An event consumed during a model
replacement is not replayed on the same frame. The field and context themselves
must remain alive through the widget call.

The original `b1f55d6` source with the new reentrant cut test failed under
ASan with heap-use-after-free (`/tmp/nexaui-clipboard-reentrant-before.log`).
An intermediate first fix timed out because replacement reset the event index
(`/tmp/nexaui-clipboard-reentrant-fixed.log` was later overwritten by a
passing repeat; the timeout is also recorded in the execution transcript).
The final test checks cut snapshot, replacement during each paste-read phase,
focus away and back, and callback execution counts. It passed in Alpine/WSL
Clang 22.1.3 ASan/UBSan with leak detection and GCC 15.2 Release Wayland-only;
it also passed GCC Debug dual-backend before the final IME-reset adjustment. An earlier
candidate also passed selected numeric, text, Wayland v3 and asynchronous
clipboard regressions (`/tmp/nexaui-clipboard-safe-asan.log`,
`/tmp/nexaui-clipboard-safe-release.log`, `/tmp/nexaui-clipboard-safe-dual.log`).
After that adjustment, the text and v3 cases passed again with ASan/UBSan
(`/tmp/nexaui-clipboard-safe-final.log`); the Release text case also passed.
These checks do not implement numeric clipboard support or close native
Aetheri clipboard acceptance; both remain open.

## Numeric visual text navigation (2026-11-05)

Source `2c77f66b27bd205d41a06d8785848404b2c8fc70` routes numeric
fields' native visual commands and physical navigation keys through the
existing general-field layout logic, retaining caret affinity, selection and
scroll state. Pointer edits and other buffer changes invalidate stale caret
state. Logical editing, numeric validation and commit remain in the existing
numeric buffer.

The widget test covers visual line edges, extended selection, replacement,
undo and physical keys. A controlled Wayland v3 IME test inserts an invalid
right-to-left draft, moves visually right within it and inserts a digit; the
committed numeric value remains unchanged. The previous `672d047` source
failed the new line-edge test (`/tmp/nexaui-numeric-visual-before.log`) and,
in an isolated copy with the new v3 fixture, failed the bidirectional
insertion check (`/tmp/nexaui-visual-before-v3.log`). The fixed numeric,
general text and three controlled v3 tests passed on Alpine/WSL in Clang
22.1.3 Debug ASan/UBSan with leak detection, GCC 15.2 Release Wayland-only,
and GCC Debug dual Wayland/X11 (five each; logs
`/tmp/nexaui-numeric-visual-asan.log`, `/tmp/nexaui-numeric-visual-release.log`
and `/tmp/nexaui-numeric-visual-dual.log`). This is controlled-server evidence,
not a human or installed-IME acceptance. Numeric clipboard integration,
other native IME implementations and Aetheri acceptance remain open.

## Numeric native IME composition (2026-11-05)

Source `e965e3bd177ff61d9ea35ee9cd246e67f9a4db95` introduces the optional
`nx_numeric_field_set_ime_window` in the 0.1.6 development package. The numeric field
uses the existing composition painting, surrounding-text and focus lifecycle,
and the general field's model/selection/buffer revision algorithm. A changed
window invalidates old replies. IME replacement can leave an invalid visible
draft; only a valid Enter changes the committed value. This supersedes the
earlier deliberate rejection of all targeted numeric IME events when the
application opts in; without a window they remain rejected.

The controlled Wayland v3 server test covers hidden-caret preedit, deletion
plus invalid insertion, multi-scalar valid replacement, valid Enter, stale
replay after model replacement, and detaching/reattaching the window with the
same text. On Alpine/WSL the v3 cases and numeric/general field tests passed
in Clang 22.1.3 Debug ASan/UBSan with leak detection, GCC 15.2 Release
Wayland-only and GCC Debug dual-backend (six cases each). V1 and the same
regressions passed on the earlier candidate before the final window-detach
invalidation. The isolated mutant omitting numeric revision checks failed the
stale replay; the pre-fix `7f653b9` source with the new detach regression failed
at the unchanged-draft assertion. Logs: `/tmp/nexaui-numeric-ime-asan.log`,
`/tmp/nexaui-numeric-ime-release.log`, `/tmp/nexaui-numeric-ime-dual.log`,
`/tmp/nexaui-ime-mutant-test.log` and `/tmp/nexaui-ime-detach-test.log`.
The mutation and pre-fix test sources are isolated and not shipped.
Static and shared installed C17 consumers also linked and called the new API
with X11 discovery disabled (`/tmp/nexaui-numeric-ime-package-final.log`);
the package test's synthetic source IDs are not provenance claims.

The controlled v3 server is not an installed input method or Aetheri consumer.
Native v1 numeric, other platforms, Aetheri input/focus/scale, IME batch failure
injection, clipboard, visual navigation and human assistive acceptance remain
open. The full T05 requirement is not complete.

## Numeric field logical commands (2026-11-05)

Follow-up `0cbc61941106a6bdb3e57d9dc2b1fc1203d91bd8` rejects
IME-targeted text and command events in numeric widgets until those widgets
publish and validate their own IME target and revision. A Release test using
the same new regression with the preceding `a37cb43` source failed when a
stale selection and text event modified the draft
(`/tmp/nexaui-numeric-negative-test.log`); the corrected numeric widget test
passed in GCC Release Wayland-only, GCC Debug dual-backend, and Clang Debug
ASan/UBSan Wayland-only with leak detection. This prevents cross-field stale
edits; it is not numeric IME composition support. An initial isolated-control
archive attempt failed because WSL could not resolve the Windows Git worktree;
the successful control used an archive created by Windows Git.

Source `3c2b336ac8cbaa7fdd11dc3485d9ed080b0097b7` routes focused
`NX_EVENT_TEXT_COMMAND` events through the existing numeric input buffer. The
widget test exercises select-all and replacement, valid and invalid commits,
undo/redo, a consumed Enter, disabled input and retry after paint failure. The
new selection/replacement assertion failed before the routing change; the
previous committed number was retained when a draft was invalid.

On Alpine/WSL, the numeric buffer, numeric widget and general text widget tests
passed in the Wayland-only Clang 22.1.3 Debug ASan/UBSan build with leak
detection and in the GCC 15.2 Debug dual Wayland/X11 build (three tests each).
The numeric buffer and widget tests passed in a GCC Release Wayland-only build
(`/tmp/nexaui-numeric-release-test.log`). An initial Release configure attempt
failed before configuration due to a shell-quoting error; the corrected run is
the one reported. These tests inject normalized events into the widget; they do
not demonstrate actual platform IME delivery. Preedit and replacement, native
clipboard integration, visual navigation and real Aetheri acceptance remain
open, so T05 is not complete.

## Aetheri virtual output mode change while Settings is running (2026-11-05)

Test driver source `9ff7e08454679754b70294ed659cfc43c50594e9` extends the
original-supervisor integration test using the existing inherited test-input
controller from Aetheri source `d621c9bba1df8ffd1083999300bcff742022b9dc`.
It does not alter Aetheri or a running carrier session. The SDK remains
`cf36802d8ef209eda91c85805e15761d0544bb26` and consumer source
`01d60fdb30e97d96778cf27cb4a2bdc9f0a0edc2`, built against its installed
Wayland-only, AT-SPI-enabled 0.1.5 static package on Fedora. The executable
SHA-256 is `5c2b0004484cb40e8badeb058423e0b5dc0d686c7939e9f71c3b4a69f51db796`.

After the original development supervisor launched the consumer, native virtual
keyboard input selected a compositor-observed output. The test controller then
requested that same headless output change from 2560x1440 to 1600x900 and
restore. The model logged the same connection-local ID 8 at both sizes, with
snapshot revisions 6, 7 and 8 and retained selection 8. The original SDL
desktop observed the resized dock. Settings then closed, reopened and stopped
in the existing owner sequence; all original child statuses and owner were 0.

An isolated test-only SDK mutant ignored mode events after an output's initial
`done`. The same driver reached the resize and failed waiting for the new
selected-output observation, while the desktop observed its resized dock.
The mutant is not shipped. A separate preliminary setup attempt failed before
the compositor started; it is retained and is not counted as a negative
control. Positive, mutant and preliminary logs/results are retained in
`D:/NexaUI/out/aetheri-model-2026-11-05/output-resize-results.tar.gz`, SHA-256
`bb78cd952aef56c3fe534ac005ae39086a4b1fedefdb6a1351abb365b894ee81`.
The mutation script is `D:/NexaUI/out/aetheri-model-2026-11-05/output-mutant.py`,
SHA-256 `82b65c4a7442760a666b3ab77686b1c6ecc4126625deeb0643a40ebe3e61e947`.
This is a virtual mode change, not output removal/addition, mixed-DPI movement,
display configuration authority, a physical monitor or human acceptance.

## Observed Wayland outputs in the Aetheri model (2026-11-05)

SDK source `cf36802d8ef209eda91c85805e15761d0544bb26` adds the
`<nexaui/wayland_outputs.h>` owner-thread snapshot and reuses the window's
existing `wl_output` objects; it opens no additional connection or permanent
thread. The 0.1.5 development package is pre-1.0. Output fields are committed
on `wl_output.done`; incomplete additions return STATE, and subscribed changes
send a coalesced native wake. Capacity failure leaves the output array intact.
Names may be unavailable, integer output scale is distinct from fractional
surface scale, and registry IDs are connection-local observations rather than
persistent device identifiers or authority.

The model consumer and integration test at
`01d60fdb30e97d96778cf27cb4a2bdc9f0a0edc2` map those observations into the
unchanged Aetheri Settings model; missing names receive a clearly generic
connection-local label. This supersedes the older record below stating that
the normal example does not acquire outputs. On the actual Aetheri headless
test compositor, its original development supervisor launched the installed
C17 consumer. The test checked the compositor-reported virtual modes
1920x1080, 2560x1440 and 3840x2160, then used native virtual keyboard input to
select the second observed ID. Existing window activation, close, reopen and
ordered owner shutdown also passed. The previous 0.1.4 consumer failed this
test at the missing three-output observation. An independent AT-SPI regression
passed. These outputs are virtual, not physical-monitor evidence.

On Alpine/WSL2, nine controlled Wayland tests passed with Clang 22.1.3
ASan/UBSan and leak detection, including pending initial `done`, v3 unnamed/v4
named output, unchanged data before `done`, mode updates, coalesced wake,
removal, undersized storage and wrong-thread handling. The private Xvfb
dual-backend test verified UNSUPPORTED. The installed model scene test passed
under the same sanitizers. Fedora GCC 16.2.1 built a static, Wayland-only,
AT-SPI-enabled, Vulkan-off SDK from the exact SDK revision above; library
SHA-256 `383b3a9c9c5413c642a6cedfaccbd872c64a4aef1432c5edc58d48d09c053e0c`,
consumer SHA-256 `5c2b0004484cb40e8badeb058423e0b5dc0d686c7939e9f71c3b4a69f51db796`.
Positive, negative and AT-SPI logs are retained at
`D:/NexaUI/out/aetheri-model-2026-11-05/outputs-results.tar.gz`, SHA-256
`290c1b663c1df46f2b54dfd2fe54849396210217c5f648125153c75f4832e276`.
Actual dynamic output addition/removal under the Aetheri compositor, output
changes during app interaction, real session/driver/physical acceptance and
configuration backends remain open; controlled removal is not a hotplug pass.

## App-owned display observations in the model view (2026-11-05)

Consumer/test source `86e4d4c0f7c4d0eeee7e5a61d6931c7f41c7b05a` adds a
display section to the existing original-model view. It distinguishes not yet
observed, observed-empty and stale states, exposes selected names and available
mode dimensions, and uses the Aetheri model's stable IDs for selection. Stale
entries are disabled. The application owner supplies snapshots and schedules
redraw; selection is not an OS display configuration. The normal executable
does not yet acquire display data. `--fixture-displays` is an explicit test
option and its two records are synthetic.

The model test used the original Aetheri model from
`d621c9bba1df8ffd1083999300bcff742022b9dc`: it selected ID 91, reordered the
snapshot, verified selection stayed on that ID, made a pending action stale,
verified the action was rejected and previous data retained, then observed an
empty snapshot. It passed on Fedora and Alpine/WSL2, including Clang 22.1.3
ASan/UBSan with leak detection on Alpine. Tests use synthetic observations and
do not prove real monitor enumeration or hotplug behavior.

On the private headless Aetheri compositor, native virtual keyboard input and
an independent AT-SPI client each selected synthetic ID 91. The existing
original-owner launch/activation/close/reopen regression also passed without
the fixture. The native consumer, built against the installed 0.1.4 Wayland-only
SDK from `41042e56d629c8532bd365212d97385b41eaa476`, had SHA-256
`4be4edb321fceec069e176f83d3697c9be916d3e29c5babf7177f3c3b725caf5`;
the independent client SHA-256 was
`a9c4a5a51404f298324a639a1725478be3e0387866d7837ddb9f4cc275b7cbad`.
The captured output SHA-256 was
`b7c1514e85bfa140c8c6505cc9284166af3b641e8aece2f073c352165046da69`.
Logs and capture are retained at `D:/NexaUI/out/aetheri-model-2026-11-05/displays.tar.gz`,
SHA-256 `e9168962fb8c00654b72a001e732a703cedb8c6a60af20cd3c60076c0e0df370`.
The [previous record](#wayland-application-id-and-aetheri-activation-2026-11-05)
identifies the SDK/compositor and test environment. Real Aetheri output
acquisition, dynamic hotplug and failure delivery, physical monitors, human
review and full Settings functionality remain open.

## Wayland application ID and Aetheri activation (2026-11-05)

Source `41042e56d629c8532bd365212d97385b41eaa476` adds the Linux
`nx_window_set_wayland_app_id` request and uses `org.aetheri.Settings` in the
experimental consumer. The pre-1.0 development package version is 0.1.4;
this is not a 1.0 release. The unchanged default is `nexaui`. The ID is
descriptive client metadata and never proves identity or authorization.

Wayland-only and dual-backend controlled-server tests on Alpine/WSL2 passed,
including default ID, replacement, 255-byte boundary, invalid/oversize input,
wrong-thread and close behavior. The Wayland-only case also passed Clang
22.1.3 ASan/UBSan with leak detection. The private Xvfb dual-backend case
returned UNSUPPORTED as required. An initial broad CTest regex also selected
an unbuilt native-management executable, so that invocation was not a full
pass; the subsequently executed exact controlled test passed. Installed 0.1.4
C17 model consumers built and ran on both Alpine and Fedora.

On the isolated Aetheri headless test compositor and original development
supervisor/SDL desktop, clicking the Settings dock entry while the consumer was
already running caused `ACTIVATED 1 1` without a second Settings spawn. Close,
reopen, owner-requested orderly shutdown and independent AT-SPI actions passed.
The otherwise identical driver against the previous consumer timed out waiting
for activation: its fixed `nexaui` ID did not match the desktop's Settings ID.
The original supervisor and fixture are unchanged and are identified in the
following record. The SDK was built from this exact source, static, Wayland-only,
AT-SPI enabled, Vulkan off; Fedora GCC 16.2.1, Wayland 1.26.0 and xkbcommon
1.13.1. Installed library SHA-256:
`752e10c0fe1dd00841af0a5ad795245461137669414448e20c43bc5b3fda9161`;
consumer SHA-256:
`42998a0f041f22a87f2a95a1561c050b35d88c4a39223afcd83e65872317f706`.
Positive, negative and AT-SPI logs are retained at
`D:/NexaUI/out/aetheri-model-2026-11-05/appid-results.tar.gz`, SHA-256
`8fb91f36d6460cbf0daa18f56017e683f9974bc333852f22949eacc95c50cea1`.
This supersedes the fixed-ID limitation in the historical record below, only
for this scoped Wayland API. Physical sessions, Windows/macOS, full Settings,
real app admission and human acceptance remain open.

## Original development owner and model-view lifecycle (2026-11-05)

Consumer and test source `8e0b8d5dbac3fc975e3bca3f03ba3768550df857`
accepts the development owner's fixed `--font`, `--cache`, `--diagnostics`
argument shape; the cache path is unused. SIGINT/SIGTERM wake the UI owner
through an application-owned nonblocking self-pipe and use ordinary teardown;
Ctrl+Q also closes the view. This is an experimental consumer, not an Aetheri
installation or change to its launcher.

The original Aetheri supervisor and its headless fixture from archived source
`d621c9bba1df8ffd1083999300bcff742022b9dc` ran with the actual SDL desktop,
test compositor and the installed-SDK consumer, supplied as separate binaries
in an isolated temporary directory. Native virtual search launched Settings,
Ctrl+Q exited it, the owner drained its original child and pipe, and search
launched a second child. After its window was observed, SIGTERM to the owner
caused the second consumer's signal cleanup, then desktop and compositor
retirement; all original children and owner exited 0. No real carrier session,
root controller or app installation was changed. The fixture reads its normal
carrier terminal-asset manifest (`ui-inputs.json` SHA-256
`67d59c720482c5e593ad616d1136994102624ded4fafaaf4057381a0b5a01319`) and the
existing reviewed font. This is a headless test adapter around the original
supervisor, not proof of an installed production session or service integration.

The first run stopped the second process before its first frame because the
driver mistakenly counted the first process's diagnostic output. Its owner
reported `settings: -15`; the driver was fixed to wait for a model line after
the second spawn and the failure remains in the archive. The final driver also
checks the desktop's window observation and the supervisor's actual JSON exit
statuses. Independent AT-SPI action and a separate owner-argument/SIGTERM
compositor run passed for the same consumer. Its binary SHA-256 is
`b43a89aecdb1f0a79b3dfa30636dd84bb891614b6584839a4714c3342c22df10`.
Logs and results are retained at `D:/NexaUI/out/aetheri-model-2026-11-05/owner.tar.gz`,
SHA-256 `7d065c30969520cde96836f57f6d9f004063e68772eb6ab4323ed403c5c30519`.
The prior record identifies the source SDK/compositor; the original owner and
fixture hashes are `f530816dbef2184293eb30b43371d8ef94888f6a78bc0c6768ccd2a24820382c`
and `13623182530bae2c08521adf09cb92ac58c3a33367f512e3152db15c35f06ee6`.

NexaUI currently hardcodes the Wayland app ID `nexaui`; it cannot yet identify
this consumer as Aetheri Settings for the desktop's app-specific matching.
Live-app activation/reuse, full app model and commands, input/IME, real services,
DRM and human acceptance remain open. A submitted frame is not app readiness.

## Original-model view reflow (2026-11-05)

Consumer source `f66d0c427ab1fecd15ad870ee3ecd7c9168ac1e1` measures
option labels at the selected text size, wraps controls and reveals the focused
control in a scrollable area rather than shrinking labels. The bounded example
adds visible mark boundaries because the high-contrast unchecked radio fill
was indistinguishable from the scroll surface in the first captured image.
This local treatment does not establish the contrast of all SDK controls.

At 480x380 logical pixels, the C17 scene test traversed all nine focus targets
with Tab at German 200%, checked each semantic rectangle was in the window,
and compared the longest option's width with an independently created 28px
text layout. It passed with DejaVu Sans on Alpine/WSL2 under Clang 22.1.3
ASan/UBSan and leak detection, and with Liberation Sans on the Fedora carrier.
A separate Clang `-Wall -Wextra -Wpedantic` build was warning-free. The test
does not prove all fonts, pixel clipping or real keyboard interaction at 480x380.

The 800x480 installed consumer ran on the private Aetheri headless compositor:
virtual native keys selected contrast, German and 200% and closed it. The
independent AT-SPI client repeated the selection and Close action. Consumer
SHA-256 was `cb11d6757bc3f48734b7b81adc89f0f973e15410caadaacf92d4ba1843ca18b4`;
the captured 1920x1080 output SHA-256 was
`4fb285df9d9f8dc62dcd79dca49110cc262c88f91f1096189dda5d7d509585dc`.
The [previous model record](#external-aetheri-settings-model-experiment-2026-11-05)
identifies the installed SDK, original Aetheri source, compositor, client and
private registry; none was replaced. Captures and logs are stored in
`D:/NexaUI/out/aetheri-model-2026-11-05/reflow-final.tar.gz`, SHA-256
`bfedff59316f0ac481fbede6f71faf295948d138ffa04fac2ec67f62b88f1604`.
This is not complete A10 acceptance: broader sizes/fonts, physical scale,
human screenreader and design review, real IME and full Settings remain open.

## External Aetheri Settings model experiment (2026-11-05)

Consumer and test source `9ff75928edcf333662593c26b59bf03effc74b66`
builds independently against the original Aetheri `src/settings/model.c` from
archived source `d621c9bba1df8ffd1083999300bcff742022b9dc`, and an installed
static, Wayland-only NexaUI SDK from
`d4c9aab9354feb4c9e045b960f4985468bec3253`. The Aetheri working tree was not
changed. The model owner retains appearance, language and text-size values
when a view is destroyed and recreated. A C17 consumer test passed on Fedora
and on Alpine/WSL2; the latter also passed with Clang 22.1.3 ASan/UBSan and
leak detection. A separate `-Wall -Wextra -Wpedantic` build produced no warnings.

On the private headless Aetheri compositor, virtual native keyboard input
selected high contrast, German and 200% text size in the original model, then
Escape closed the consumer. The independently captured 1920x1080 output SHA-256
was `c30e207cefc612a4af599938a62e5ea86b0c039c893315c37c5b3444eaeaa0e1`.
An independent libatspi client on the same compositor and a private registry
selected the same three radio controls, observed their checked states and
closed the app through its localized button. The AT-SPI-disabled consumer failed
at application discovery as expected. An initial positive run exposed that the
consumer drained window events without routing native accessibility actions;
the tested source uses `nx_window_route_events` and the repeated run passed.

The compositor, AT-SPI-enabled consumer and independent client SHA-256 values
were respectively `da09d434747dafc3400fb8da4f426b3cc8713452515d61e2dd9d3dbf693ed164`,
`d9abf8b7b769aae12aaad81b4a77b5fe2430637075d87e880b13e953b97f618b`, and
`23a2ba88d6770d9de78e161253cea0a9a87fcfb66705a3b737d66be7afb35787`.
The positive, negative and initial failing logs and capture are retained in
`D:/NexaUI/out/aetheri-model-2026-11-05/results.tar.gz`, SHA-256
`a49831ea3b3ebe0075d43b3c094fb3431834c33f939ced04f1cee6fe05e5fa5d`.
This covers a limited model/view seam, not the full Settings consumer: Aetheri's
process owner, display observation, service wakeup, IME field, target-image
accessibility service, persistence, physical output and human review remain
open. The layout and fitting are provisional; no app or OS settings are saved.

## Settings AT-SPI alongside Aetheri's SDL desktop (2026-11-05)

Driver source `82ca6abf155dd4c9a3c30724a669a7e372b6f904` combines the
previously recorded private Fedora AT-SPI registry, independent libatspi client
and installed Settings consumer with the actual SDL desktop on Aetheri's
headless test compositor. The independently archived Aetheri source remains
`d621c9bba1df8ffd1083999300bcff742022b9dc` and the installed NexaUI SDK
remains `d4c9aab9354feb4c9e045b960f4985468bec3253`; the running Aetheri
checkout and original session were not modified.

The client discovered the Settings app, exercised its Text/EditableText,
caret/selection, checkbox and Apply model paths, and observed a clean host
exit. The SDL desktop observed one window and its later removal; desktop,
test runner and compositor exited 0. An otherwise identical run using the
AT-SPI-disabled Settings binary failed discovery, as expected. The launcher
channel was held open by the driver: Settings was started by the independent
client, not by Aetheri's process owner. No Aetheri Settings model is used.

The exact host, client, registry and compositor hashes are the same as the
preceding AT-SPI record; the SDL desktop SHA-256 is
`68814d3f4c03bd1285e3a1ea77dee27ae229d60989ee3c751fecaa57ef8edac9`.
The positive and negative logs are retained in
`D:/NexaUI/out/aetheri-desktop-atspi-2026-11-05/results.tar.gz`, SHA-256
`458cf25a937d4f57fd8494f2b6bbd8672198bb808528173bef81fe312373853b`.
The registry and D-Bus dependencies are the isolated, signature-checked
Fedora prefix described below. This test does not establish Aetheri target
image services, shared desktop accessibility, IME, physical operation or
human screenreader acceptance.

## Independent AT-SPI on the Aetheri compositor (2026-11-05)

Driver `tests/run_aetheri_atspi.py`, source
`91caaead6b6d57bb6b94560ff1fb3608f030c074`, runs the existing independent
libatspi Settings client and a private D-Bus/AT-SPI registry against Aetheri's
headless test compositor `d621c9bba1df8ffd1083999300bcff742022b9dc`. The
Settings example was copied and separately built against an installed static
Wayland-only SDK from `d4c9aab9354feb4c9e045b960f4985468bec3253`, this time
with AT-SPI enabled. The real independent client exercised Unicode Text and
EditableText, caret/selection operations and notifications, checkbox and Apply,
observed the resulting model, and the app closed. Registry, client and
compositor completed successfully. The AT-SPI-disabled build failed the same
client at application discovery, confirming that an absent semantic provider
is not counted as success.

The Fedora 44 carrier did not have the required development files or AT-SPI
registry installed. The test used a separate temporary prefix extracted from
Fedora-signed RPMs: at-spi2-core and devel 2.60.7, D-Bus daemon/devel/tools
1.16.2, glib2-devel 2.88.0, libXi 1.8.3, libXtst 1.2.5 and libXext 1.3.6.
All nine signatures and digests passed the carrier's `rpmkeys --checksig`.
The registry and its private libatspi depend on X11 client libraries even
though the NexaUI window backend is Wayland-only; those are test runtime
dependencies, not evidence of an X11 window backend. No RPM was installed.
The independent client was built separately with explicit private include and
library paths because the carrier lacked the complete GLib development
`pkg-config` dependency chain. The initial download from the Fedora redirect
returned HTML and the first mirror had a certificate name mismatch; neither was
used. Original Fedora packages came from the OSUOSL Fedora mirror and were
verified on the carrier before extraction.

Host, independent client and registry SHA-256 values are respectively
`5e7a5b4b9e62ffb67b524124e64a1d9a90389d343829cc4f17ac0858dff08856`,
`251f28d0a4d97c8cee7ce1e4177e4e710dbcfa436c3ff9f284dddd9a0b893505` and
`fba13406b5e8ebe67c6eea1aa99f61d2b67a2620ba01bac74ce2772261dd9324`.
Positive and negative logs are retained in
`D:/NexaUI/out/aetheri-atspi-2026-11-05/results.tar.gz`, SHA-256
`ed62865cb537173b64e941f404fdd6b3d89bf717d47bca19f09773d93183d24a`.
This private-registry, headless result does not establish the Aetheri target
image's accessibility services, actual desktop/session integration, IME,
human screenreader use or physical accessibility acceptance.

## Aetheri SDL desktop with installed NexaUI consumers (2026-11-05)

Driver source `374e69da19ef9708068bdb3ab808254d603a024c` exercises the
independently built actual Aetheri SDL desktop from source
`d621c9bba1df8ffd1083999300bcff742022b9dc`, on the private headless test
compositor and test-input driver from that same source. The installed static
Wayland-only, software-only NexaUI SDK remains
`d4c9aab9354feb4c9e045b960f4985468bec3253` with AT-SPI disabled. The desktop
uses the existing reviewed SDL3/text prefix and a private shader cache; its
launcher channel is retained by the driver but no app-launch request is made.
The consumers are started independently, not by Aetheri's lifecycle owner.

The actual desktop creates compact dock surfaces on all three outputs. A public
SDK probe observes 1840×916 initial, 1920×996 maximized and 1840×916 restored;
first-output pixels show the dock, window and work-area boundaries in each
state. The desktop observes the probe's close. A separately built installed
Settings example then maps on the second output; captured pixels show its
content, and virtual native keys traverse focus 3/4/5 and produce the expected
applied model state. The desktop observes its arrival and departure. Both
consumers, the SDL desktop and compositor exit 0; the private socket disappears.
An earlier SDK `2e4e8e9ffa0e0244b9a6bbecbe73f7fc43e91c65` instead submits
2100×1200 and the same driver fails waiting for the required initial size.

The SDL desktop executable SHA-256 is
`68814d3f4c03bd1285e3a1ea77dee27ae229d60989ee3c751fecaa57ef8edac9`.
The locally retained archive
`D:/NexaUI/out/aetheri-sdl-2026-11-05/positive.tar.gz` has SHA-256
`5cf2723e53b257e7e164370c18a1ccb48a487aa455b9174c3c609af92b2d6bee`;
its Settings image independently re-hashes to
`28f5fa4110739c89e5d499e7e811d9077f3e431be8e8c817fd0d53c4cf8e3e9a`.
This is headless software composition and synthetic input: it does not prove
actual owner-mediated launch, Aetheri-backed Settings models, IME or AT-SPI,
mixed scaling, DRM/physical input, or human accessibility acceptance.

## Aetheri dock and ordinary NexaUI window (2026-11-05)

Test source `7598f34c0f5d7ce0002a1d831316ba5a709031d9` adds a strict C17
consumer of the installed public SDK and an isolated driver for Aetheri's
test compositor and its private-role test desktop. The same independently
built Aetheri `d621c9bba1df8ffd1083999300bcff742022b9dc` compositor and
installed Wayland-only static NexaUI `d4c9aab9354feb4c9e045b960f4985468bec3253`
SDK from the preceding carrier run were used; Aetheri's separate work was not
changed. Its test desktop actually maps the 84-pixel exclusive dock on three
headless outputs. It is a solid-color protocol fixture, not the SDL desktop.

On the first 1920×1080 output, the consumer requests 2100×1200 and observes
1840×916 logical pixels and the same buffer extent, matching the dock and
40-pixel margins. It then observes 1920×996 when maximized and 1840×916 on
restore. Three compositor captures check the app, reserved background and dock
pixels; both repeated runs have matching hashes. The consumer closes, the
private desktop receives `retired`, all three processes exit 0 and the private
socket disappears. With the same consumer built against the earlier SDK
`2e4e8e9ffa0e0244b9a6bbecbe73f7fc43e91c65`, the first submitted frame instead
reported 2100×1200 and the check failed. This negative run's cleanup also
reported connection loss after the test stopped the processes; it is not a
clean-retirement result.

The successful log and first-output captures are retained at
`D:/NexaUI/out/aetheri-dock-2026-11-05/positive.tar.gz`, SHA-256
`8caae3eb0ec45de849f6249171e4571db1e4aec2983df695e447bf72dbc8199f`;
the three extracted image hashes match the recorded run. Headless test roles,
solid-color content and 1× output do not establish actual SDL dock integration,
physical placement, mixed DPI, accessibility or complete Settings adoption.

## Aetheri headless compositor and installed Settings consumer (2026-11-05)

An isolated Fedora carrier run combined NexaUI source
`d4c9aab9354feb4c9e045b960f4985468bec3253` and the independently archived
Aetheri source `d621c9bba1df8ffd1083999300bcff742022b9dc`. Both archives'
SHA-256 values matched after transfer. The Aetheri checkout's concurrent input
and IME changes were not copied or altered. On the running QMP-identified
carrier, both were built in a separate temporary directory with GCC 16.2.1,
Wayland 1.26.0, xkbcommon 1.13.1 and wayland-protocols 1.49. Aetheri used
its existing wlroots 0.20.2 development prefix. NexaUI used a static
Wayland-only Release SDK, software rendering, Vulkan off and AT-SPI off; the
Settings example was copied and built separately with `find_package` against
that installed SDK.

The reusable driver is `tests/run_aetheri_headless.py` at source
`beba147e02592ee5e15a89c1be969a2f458fccab`. It starts the independently
built Aetheri **test** compositor on an owned headless socket and injects
keyboard events through its inherited test-only input driver. The installed
consumer mapped, submitted a software frame, traversed focus IDs 3/4/5,
observed the applied model state, closed successfully, and the compositor
stopped successfully. A captured 1920×1080 first output contained the
Settings pixels; the other two capture files were produced but their pixels
were not checked. Test compositor SHA-256:
`da09d434747dafc3400fb8da4f426b3cc8713452515d61e2dd9d3dbf693ed164`;
Settings executable: `574e3dac048f091c042a86b344614dbb0a5faf167279125642eda9ffcb83ee93`;
first-output PPM: `c35004aa57f1442aa47a950c68fe4c1c1ab1596f261dd1dabbeb882684204a56`.
The downloaded log/image archive is retained at
`D:/NexaUI/out/aetheri-headless-2026-11-05/installed-final.tar.gz`, SHA-256
`5eccb3f4778e4a6e4452b9e171e1fda922c257b628eb5da63cc72a253608156e`;
the extracted image hash was independently rechecked. An input-omission
counterexample timed out and returned failure. Two setup attempts are not
counted: one redundantly added the already-present test device, and another
closed the test socket before orderly server shutdown.

This is actual Aetheri compositor code, but a private headless test build with
virtual input, not the DRM desktop or its private dock role. It does not prove
the new bounds against a dock, live Aetheri app/service models, IME, AT-SPI,
physical devices, mixed scale, installed shared SDK or human accessibility.

## Initial Wayland window bounds (2026-11-05)

Source `792520c509935fb6f77231546b90e3b4d6f1eb20` uses xdg-shell v4+
`configure_bounds` for the first normal window configure when the compositor
leaves the corresponding dimension to the client. Explicit compositor sizes,
later interactive resize, maximize and fullscreen are not clamped by this
initial advisory policy. Unknown bounds preserve the requested dimension;
the chosen normal size is restored after maximization. This addresses the
Aetheri development compositor's documented dock/work-area recommendation,
without changing the compositor, its shell or its process owner.

The controlled native protocol server checks both dimensions, an unknown
dimension, explicit configure precedence, zero bounds, the attached shared
memory buffer size, subsequent resize, maximize/restore and an xdg-shell v3
server. GCC and Clang ASan/UBSan Wayland-only runs passed. The new test failed
on the former adapter's ignored initial bounds in an isolated source copy.
Three v3 text-input tests, async clipboard and group-transfer tests passed;
after rebuilding the Settings consumer, its small-window keyboard test and
all four explicitly enabled Weston simple-IME cases passed. The initial IME
attempt was skipped and the first dedicated runner lacked `setxkbmap`; neither
counts. Final private runs are `/tmp/nexaui-bounds-native-small/result.json`
and `/tmp/nexaui-bounds-simple-ime-final/simple-ime.xml`.

The separately evolving Aetheri checkout was at committed
`d621c9bba1df8ffd1083999300bcff742022b9dc` with concurrent uncommitted input
and IME work during inspection; it was not modified. No runnable Aetheri
compositor or wlroots development package was found in this Linux environment,
so the Aetheri work-area, actual pixel placement, mixed scaling and human
acceptance are still unverified. Weston and the controlled server do not replace
those checks.

## Bounded outgoing Wayland clipboard transfers (2026-11-05)

Source `a7bfc0964ee2773f47c1564d03b8bde462d5b021` gives each native
outgoing clipboard request a five-second total deadline, included in both
single-window and group waits. Partial writes do not extend it. At expiry the
owner closes the pipe and releases the slot while polling; the receiver can
have received a prefix, and neither closure nor a complete write establishes
delivery or revocation. Existing limits and owner-thread ownership remain.

Two controlled Wayland-server tests each fill four pipes, drain them after two
seconds, verify expiry around the original deadline, then verify that four new
requests can make progress. They passed GCC and Clang ASan/UBSan Wayland-only
builds; with the previous adapter in an isolated dual-backend source copy, both
failed the time bound after nine seconds. Ten clipboard cases and the separate
async-read and group-transfer cases passed in the GCC build. After rebuilding
the examples against the changed static library, the native Settings keyboard
clipboard and cell-grid clipboard tests passed on private Weston/Xvfb
(`/tmp/nexaui-send-native-settings-final/result.json` and
`/tmp/nexaui-send-native-grid-final/result.json`). The earlier native run
before relinking does not count. These checks do not establish Aetheri
compositor acceptance, delivery to a real slow application, full clipboard
interoperability, or physical and human accessibility acceptance.

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
# External-host committed text seam — 5 November 2026

Candidate based on `bcd3859849f24a41fb0347b94c8589f7f9436c07` adds the
0.1.12 `nx_text_field_set_host_ime` and `nx_text_field_host_ime_revision`
contracts for ordinary fields on host-owned surfaces. This advances T04/T06,
I02 and D05; it does not finish native IME or the private desktop adapter.
The host must forward surface focus and manage native protocol lifecycle,
preedit, candidate positioning and once-only replies. Password fields remain
unsupported. See the external-host text-input section in `docs/api/core.md`.

On Alpine/WSL2, the static Wayland-only GCC text-field test passed, including
tagged deletion and multi-scalar commit, stale replay, selection and focus
round trips, policy changes and an incompatible window binding. The same test
passed with GCC shared and Clang ASan/UBSan with leak detection. Four existing
controlled Wayland v1/v3 input tests passed. A copied desktop consumer built
against the installed 0.1.12 static SDK and passed its synthetic tagged-commit
test with DejaVu Sans; the older 0.1.11 SDK was rejected at configuration. A
separate negative-control source with host event matching disabled failed the
positive commit test. The initial copied-consumer invocation used a nonexistent
font path and failed; the corrected path passed. A broad test-name filter also
selected an unbuilt unrelated timeout test; after building that target it
passed separately. Neither is evidence for native host preedit, physical input,
an actual input-method provider or accessibility acceptance.

## External-host preedit and caret — 5 November 2026

Candidate based on `2fd65e128a3f0b746c6e56114e65824192f9b13f` adds
0.1.13 host-preedit and drawn-caret queries for ordinary text fields (T04,
T06, I02 and D05). The copied draft is associated with a current field token,
validated and limited to 4096 bytes. Painting is separate from the committed
model and semantic value; text, selection, focus or policy changes prevent stale painting. The
host still owns native context activation, provider and event sequencing,
candidate placement and cancellation. There is no native host connection in
this candidate and no password-field support.

Alpine/WSL2 GCC static/shared and Clang ASan/UBSan with leak detection passed
the text-field test, including changed caret geometry, clear, caller-buffer
ownership, invalid UTF-8/offsets, stale token and unchanged semantic value.
The desktop consumer built against the installed static SDK and passed a
synthetic preedit and commit test. Four rebuilt controlled Wayland v1/v3 input
tests passed. In a separate test-only source copy, disabling host-preedit
painting made the caret movement assertion fail. These are not installed IME,
physical input, Aetheri host-protocol or human accessibility acceptance.

## Drop fixture cleanup — 5 November 2026

The [GitHub sanitizer job for b009c9f](https://github.com/Lulus792/NexaUI/actions/runs/42065028791/job/127376677806)
failed its reverse-driver run: LeakSanitizer reported 18,752 bytes in 23
allocations from a client of the controlled Wayland drop test server. The same
test had passed earlier in the other driver order. This is a server-fixture
lifetime finding, not proof that production clipboard or drop resources leaked.

On candidate base `8b13235`, the backpressure test deterministically pauses
the server before closing the client. With the old teardown, Clang ASan/LSan
reproduced the same 18,752 bytes / 23 allocations and exit 1. After joining the
server thread, the fixture now destroys connected clients before the display,
as specified by the [Wayland server API](https://wayland.freedesktop.org/docs/html/apc.html).
The deterministic test then exited 0 with leak detection enabled and passed
10 consecutive runs. All 13 controlled async-drop cases passed in static,
shared and Clang ASan/UBSan configurations on Alpine/WSL. The old GitHub run
remains failed; a successful local correction does not replace a new CI run.

## Original Aetheri shell and PTY view lifetime — 5 November 2026

On NexaUI base `6c44a9abc9a3fd25901f71ec2dad76579b68affe`, the optional
[terminal consumer experiment](../../examples/23_aetheri_terminal_view/README.md#original-shell-and-pty-lifetime-experiment)
uses the model, PTY and shell from Aetheri revision
`87996e2f12128b172a80b325ad3b539df6aee527`. Actual shell output reached the
model and a rendered text view; a second command succeeded after view
destruction, and a replacement view observed its output. The test separately
observed the shell's clean exit and PTY EOF before closing the PTY. Static,
shared and Clang ASan/UBSan runs passed on Alpine/WSL as an unprivileged user;
a `/bin/false` replacement was rejected.

This environment required optional test-local syscall wrappers. Without the
separately enabled `tcgetattr` output normalization, the shell reported
terminal-mode drift and exited 125. With that normalization it passed; the
underlying cause remains a hypothesis, and this is not a pass for unmodified
Aetheri on musl or for its glibc target. Actual process-owner and native-input
integration, IME, clipboard-to-PTY delivery, session policy, physical input and
human assistive acceptance remain open.

## Independent clipboard to original Aetheri PTY — 5 November 2026

On NexaUI base `105b9f67dc73649cfe908e37e8d63765ae90c172`, the optional
[native paste/PTY experiment](../../examples/23_aetheri_terminal_view/README.md#independent-wayland-clipboard-to-the-original-pty)
combines an independent Wayland clipboard owner, the existing full paste review
and the Aetheri shell, PTY and terminal model from revision
`87996e2f12128b172a80b325ad3b539df6aee527`. A confirmed controlled command
produced a split marker in actual shell output. A fixed subsequent shell probe
completed without the offered marker after pending cancellation, review
cancellation and invalid control content. The shell exited and the PTY reached
EOF. Static, shared and Clang ASan/UBSan private Weston/Xvfb runs passed as an
unprivileged user; the existing no-PTY clipboard regression also passed.
An isolated mutant suppressing the confirmed PTY write failed because the
expected shell-output marker was absent.

The Alpine runs enabled both test-only musl compatibility and the separately
documented termios normalization. The synchronous PTY helpers are for the
bounded fixture only. Aetheri's real process owner, native IME, app event-loop
backpressure, glibc target, physical input and human assistive acceptance remain
open; this result does not move shell or session ownership into NexaUI.

## Terminal composition position adapter — 5 November 2026

On NexaUI base `47d7436d8e91edcafd8b46c0484c1426f81a9812`, an optional
[terminal IME adapter experiment](../../examples/23_aetheri_terminal_view/README.md#terminal-preedit-adapter-experiment)
converts NexaUI preedit UTF-8 byte offsets to the original Aetheri composition
model's scalar positions, with explicit unsupported handling for an adjacent
reverse selection the original model cannot represent. Static, shared and
Clang ASan/UBSan tests covered multibyte positions, hidden caret, draft
validation and simulated stale owner guards. A controlled Wayland v3 server
delivered actual protocol preedit through NexaUI to that model. A mutant using
byte positions directly failed the multibyte test.

The caller must still fence untagged native preedit notifications across focus
and session changes. Preedit drawing and atomic whole-commit validation and
delivery to the app-owned PTY are not implemented here. Installed IME, actual
Aetheri, physical input, scale and human assistive acceptance remain open.

## Wayland IME commit boundaries — 5 November 2026

On NexaUI base `006cc663cf38111e8fcdebc18718ac79a64e89cb`, version 0.1.16
adds START/END and filtered-content flags to existing Wayland v1/v3 scalar
text events without changing event count or `nx_event` size. The terminal
example buffers a bounded, consistently targeted sequence and validates the
whole commit using the original Aetheri model before returning any bytes.
Static, shared and Clang ASan/UBSan Wayland v1/v3 protocol tests passed,
including an injected C0 character. The installed example passed valid and
filtered controlled-v3 tests in all three configurations; stale targets,
interruption and over-limit commits are also covered. A mutant which ignored
the filtered flag failed the controlled-v3 rejection check.

The flags preserve the legacy filtering behavior and expose when it happened;
an all-filtered commit produces no text event. They do not delimit IME delete
transactions, guarantee preedit focus fencing or implement PTY writes.
Installed IME, real Aetheri, physical input and human assistive acceptance
remain open.

## Guarded Wayland preedit snapshots — 5 November 2026

On NexaUI base `21e90dd44606ef4d332b42f2f08a0d31f373983a`, version 0.1.17
associates Wayland preedit notifications and snapshots with the advertised
target and revision. The new guarded getter rejects snapshots whose identity
does not match both the request and the currently advertised editor. The
terminal example checks the notification and getter before updating Aetheri's
composition model. In the controlled v3 test, changing the revision invalidated
the old getter and an old notification could neither adopt nor clear the new
draft. A v1 test checked a valid snapshot and wrong-revision rejection. Static,
shared and Clang ASan/UBSan tests passed; a mutant ignoring event identity
failed the stale-notification case.

The owner remains responsible for advancing epochs and clearing its draft on
focus/session changes. Other platforms report the guarded getter unsupported.
Real Aetheri focus transitions, installed IMEs, preedit rendering, PTY input,
physical interaction and human assistive acceptance are not established by
these controlled tests.

## Aetheri terminal composition overlay — 5 November 2026

On NexaUI base `99c2787b2f1d6855900265220d6f3b57bc4365f6`, the optional
[composition overlay](../../examples/23_aetheri_terminal_view/README.md#composition-overlay-on-the-terminal-grid)
paints the original Aetheri composition viewport through the existing cell
grid and returns logical candidate-caret geometry. The confirmed terminal
snapshot remains unchanged. Static/shared and Clang ASan/UBSan tests covered
selection, wide and combining cell placement, caret visibility, long-draft
scrolling, stale guards and raster scales 1, 1.5 and 2. A mutant omitting the
overlay failed. A synthetic headless frame was inspected; its test font lacked
the CJK glyph, so the missing-glyph box remains a documented font-fallback
gap, not successful script coverage.

Accessible preedit publication, actual native candidate placement, installed
IME, production Aetheri integration, physical input and human visual and
screenreader acceptance remain open.

## Terminal glyph fallback — 5 November 2026

On NexaUI base `6cf6baf7fc7ad2734ed6661f092e10b4d0b438e0`, the external
[terminal example](../../examples/23_aetheri_terminal_view/README.md#ordered-fallback-for-terminal-and-preedit-glyphs)
uses existing ordered font collections for missing regular and bold glyphs in
both the cell grid and composition overlay. No configured match fails the
frame explicitly; absent collections retain the original `.notdef` behavior.
An installed SDK test used local DejaVu Sans Mono and Noto Sans JP TTF files:
the first lacked U+754C and the second contained it. Static/shared and Clang
ASan/UBSan runs verified selected faces, unsupported coverage, unchanged
legacy behavior and a changed headless overlay. The synthetic frame was
visually inspected; a mutant ignoring fallback failed the face assertion.
Font hashes and bounds are in the example README. No font was bundled.

One glyph and font pair do not establish general shaping, bidi, cross-font
mark placement, font discovery, visual quality on Aetheri, or human assistive
acceptance. Those gates remain open.

## Read-only terminal composition accessibility — 5 November 2026

On NexaUI base `1c915a65e3d10027500efbd2dc2d59c879eac3c3`, the optional
[terminal composition snapshot](../../examples/23_aetheri_terminal_view/README.md#read-only-accessible-composition-snapshot)
publishes the full uncommitted draft separately from the existing terminal
text, with app-supplied identity, optional localized name, actual selection and
optional caret. An independent libatspi client using a private registry and
simulated host verified text, range, hidden caret, change, removal and rejected
mutation; it also verified the confirmed terminal content was unchanged.
Static/shared and Clang ASan/UBSan runs passed. A mutant publishing different
text failed the client, and existing terminal accessibility tests passed.

Publication is opt-in and must be disabled by the owner for sensitive input.
Real native focus, automatic announcement, installed IMEs, production privacy
handling and human screenreader acceptance are not demonstrated by this host.

## Controlled terminal IME candidate placement — 5 November 2026

On NexaUI base `d09c971ce6cc3b6fa19d434c4a8d4ff5c5e32fff`, the optional
[terminal candidate-placement test](../../examples/23_aetheri_terminal_view/README.md#candidate-position-through-the-native-window)
translates Aetheri composition-caret geometry by the grid's logical window
origin and forwards it through the existing native IME rectangle API. The
controlled Wayland v3 server observed the rectangle after draft updates,
terminal movement, resize and negative-origin rounding. A stale epoch and a
non-finite origin caused no update. Static/shared and Clang ASan/UBSan tests
passed, as did the existing v3 regressions; a mutant dropping the window origin
failed the coordinate check. Render scales 1, 1.5 and 2 were exercised without
claiming an actual native fractional-scale output.

Installed IME candidate presentation, real Aetheri chrome and focus, physical
output changes and human visual acceptance remain open.

## Reviewed clipboard paste through a queued PTY — 5 November 2026

On NexaUI base `5c844d726f268b465846b5f8b291ab2c75e7e227`, the installed
native paste/PTY test consumer copies the confirmed full snapshot to the
existing bounded example queue and uses the Linux descriptor readiness API
to drain it in the window loop, which retains its 50 ms test timeout. It checks original Aetheri
model output and owns the child process. The independent `wl-copy`/`wl-paste`
driver passed pending cancellation, review cancellation, fresh acceptance and
control-byte rejection with static, shared and Clang ASan/UBSan builds. In the
accept case it also stopped the original shell and filled its PTY before the
clipboard review, then resumed it after confirmation. A mutant suppressing
queue drain failed on the missing output. The no-PTY native regression passed;
the installed 0.1.16 package still built the no-PTY target while the new
PTY target correctly required 0.1.18.

This is private Weston/Xvfb with virtual input, original Aetheri revision
`87996e2f12128b172a80b325ad3b539df6aee527`, and test-local Alpine/musl
wrappers. The shell resumes immediately after confirmation, so no specific
number of blocked writes after confirmation is claimed. Shutdown and the
cancelled-paste probe retain bounded synchronous test helpers. Production
session switching, partial-prefix policy, IME under the same pressure, actual
Aetheri compositor and human acceptance remain open.

## Original Aetheri shell under PTY backpressure — 5 November 2026

On NexaUI base `8c77c8bab7676ff7213d1b23f6ef3edf7cc9fb56`, the installed
consumer `aetheri_terminal_pty_backpressure_test` opens the original Aetheri
shell revision `87996e2f12128b172a80b325ad3b539df6aee527` before creating
its window. Once its prompt is observed, the fixture stops only its owned
child and fills its PTY with no-op input until a nonblocking write returns
`EAGAIN` and the window wait reports no write readiness. It queues two
separately identifiable commands and an exit request, resumes the shell,
and drives the queue through the public single-window wait. Both actual
shell result markers appeared once and in order, and exit status and PTY EOF
were observed. The static, shared and Clang ASan/UBSan cases passed on private
Weston/Alpine; an isolated reversed-command mutant failed the ordering check.

An initial negative-control attempt failed earlier because the PTY driver
could free space between the wait and an extra direct write. That extra write
was removed; the final test writes queued data only after readiness. The
fixture uses virtual display and process stopping and the documented musl
test wrappers. It does not establish physical interaction, an unmodified
glibc build, arbitrary shell workloads or Aetheri's production session policy.

## Controlled PTY backpressure — 5 November 2026

On base `00649d379c10bc4565daaf63e6faad8db275e2f2`, the native IME test
consumer uses its bounded example-owned PTY queue. The existing controlled
Wayland group-wait test fills a Linux PTY master to `EAGAIN` with a raw-mode
slave that initially does not read. It queues two items, verifies no write
progress while blocked, and checks every byte after the slave starts reading.
It also forces short writes and `EINTR`, `EAGAIN`, and `EIO`, exercises atomic
overflow rejection, compaction and clearing. Static, shared, and Clang
ASan/UBSan runs passed. An isolated mutant that failed to advance the queue
offset exited 1 at the byte-order check. The real private Weston IME commit and
focus-cancellation tests with the original Aetheri shell passed again in all
three configurations; those short commits did not fill the PTY. The delayed
raw-mode reader is a controlled peer, not an Aetheri shell, and full owner
session-switch and shutdown policy remain open.

## Linux descriptor write readiness — 5 November 2026

On base `1b8020e694236d3a2151345ea07064040b9e87fa`, the 0.1.18 candidate
adds explicit read/write interests for a borrowed Linux application descriptor
to single-window and Wayland multi-window waits. The earlier functions remain
read-only. A controlled socketpair test filled a nonblocking send buffer to
`EAGAIN`, verified that writability was absent, then checked that a delayed
peer drain woke the group wait. It also checked simultaneous read/write,
invalid-descriptor error, hangup and legacy read-only behavior. Static, shared
and Clang ASan/UBSan variants passed. The single-window API was additionally
exercised with native Wayland and X11 windows. On private Weston, the existing
group wait fault-injection, owner-thread, reentry and peer-destruction tests
also passed in the static build. Four controlled text-input-v3 regressions
passed in each of the three build configurations.

The installed native terminal test consumer now queues its single validated IME
commit and writes it only on readiness; private Weston simple-IME commit and
focus-cancellation cases passed in all three configurations with Aetheri shell
revision `87996e2f12128b172a80b325ad3b539df6aee527`. Alpine/musl test wrappers
remain necessary for that shell. These short commits do not exercise actual PTY
backpressure, multiple commits, session switching, or production shutdown. The
fixture still uses a bounded synchronous helper to terminate the owned shell.

## Private Weston simple-IME to Aetheri PTY — 5 November 2026

On NexaUI base `2f3b3af20af80918738b5dbeef13fb8b9d0fbb19`, an optional
[native terminal test](../../examples/23_aetheri_terminal_view/README.md#private-weston-simple-ime-to-the-original-pty)
used a private Weston 14.0.2/Xvfb seat, its separate `weston-simple-im`
process, virtual keys and the original Aetheri shell/PTY/model at revision
`87996e2f12128b172a80b325ad3b539df6aee527`. In static, shared and Clang
ASan/UBSan builds, the app rendered a preedit, forwarded one validated Unicode
commit, observed it in PTY output and shut down cleanly. A second case moved
focus away during composition, returned and observed only a new commit. The
native trace showed the IME and candidate-rectangle requests. An isolated
mutant suppressing the PTY write failed the shell-output check.

An earlier forced-focus run of the initial consumer failed preedit adoption
after queued focus changes; synchronizing the final queued focus state allowed
the refocus case to pass. Both Alpine syscall and termios test adapters were
enabled. This does not establish general Wayland-v1 focus correctness,
production Aetheri integration, chosen IME behavior, physical input/output,
native candidate placement quality or human visual/screenreader acceptance.

## Controlled Wayland IME to original Aetheri PTY — 5 November 2026

On NexaUI base `032bfeccdd546b34d500bbd6f15b59eeb3e19f9c`, an optional
[terminal test](../../examples/23_aetheri_terminal_view/README.md#controlled-wayland-ime-to-an-aetheri-pty)
starts the Aetheri shell and PTY from
`87996e2f12128b172a80b325ad3b539df6aee527` before the controlled Wayland
server thread. A complete tagged v3 IME commit is validated and written to the
owned PTY. A separately simulated Enter executes the safe test command; its
split marker is observed in the original terminal model. Filtered and stale
commits are rejected, and an independent fixed command confirms the shell
continues without the rejected marker. Static, shared and Clang ASan/UBSan
Alpine/WSL runs passed as an unprivileged user with the separately documented
test-only musl syscall and termios adaptations.
An isolated mutant that discarded the approved commit failed the expected
shell-output check.

This is not installed-IME or real Aetheri compositor acceptance. The fixture's
synchronous PTY helpers are not a production event loop; native foreground
and focus policy, preedit painting, physical input, and human accessibility
remain separate gates.
