# Aetheri catalogue: preliminary requirement-to-source map

I02/T05/A02/Q06: a pending independent AT-SPI Apply request and Wayland IME
preedit were exercised together in a private numeric host. The request was
accepted against the old snapshot but not applied after the guard frame; a
subsequent disabled action was rejected. This is not a general event-loop
barrier or human assistive acceptance. See [candidate evidence](aetheri-adoption-assessment.md#pending-at-spi-action-and-ime-preedit-in-one-batch-2026-11-06).

I02/T04/T05/Q06: v1 preedit now tolerates caret-only serial advances while
checking the editor snapshot. An isolated numeric host demonstrated a guard
frame when observed preedit and pointer press share a native input batch; this
does not establish a general host/AT-SPI event barrier. See [candidate evidence](aetheri-adoption-assessment.md#wayland-v1-preedit-serial-and-queued-pointer-input-2026-11-06).

I01/I02/T05/Q06: pointer presses on a disabled target now retain focus and
cannot click through. A published disabled Apply frame preserved an active
simple-IME preedit across a native pointer click; same-batch state transitions,
other providers and human acceptance remain open. See [candidate evidence](aetheri-adoption-assessment.md#disabled-pointer-target-preserves-composition-focus-2026-11-06).

T05/I01/I02/Q06: a labeled numeric Apply test view accepts native keyboard
confirmation and, in a separate Weston simple-IME session, a pointer action
after a verified IME digit. Uncommitted-preedit clicks, keyboard under that IME,
actual Aetheri and human acceptance remain open. See [candidate evidence](aetheri-adoption-assessment.md#visible-numeric-apply-with-keyboard-and-ime-input-2026-11-06).

T05/A02/Q06: a separate libatspi client invokes a numeric Apply button in the
private Wayland semantics fixture, including invalid draft and disabled-action
negatives. The case is not a visible or IME-driven user workflow; keyboard,
active composition and human acceptance remain open. See [candidate evidence](aetheri-adoption-assessment.md#native-accessibility-action-for-numeric-confirmation-2026-11-06).

T05/I02/Q06: `nx_numeric_field_commit` in the 0.1.20 candidate provides an
explicit application-controlled confirmation path for a validated draft. A
test-only owner action passed after native simple-IME input in static/shared/
sanitized profiles; keyboard access under that provider and composition policy
remain open. See [candidate evidence](aetheri-adoption-assessment.md#explicit-numeric-confirmation-after-native-input-2026-11-06).

T05/I02/Q06: the numeric simple-IME test on base
`d799663d28886f567d096dbe4bf27a94c2542ef3` now checks a native valid digit
and preservation of the previous confirmed value before preedit and invalid
commit. A separate strict Enter-to-confirm attempt failed with this provider;
see [candidate evidence](aetheri-adoption-assessment.md#numeric-valid-draft-and-ime-confirmation-gap-2026-11-06).
Reliable keyboard confirmation under that IME remains open.

B03/W01/W04/I01/A02/A10/Q04: the unchanged Aetheri-owned Settings host and
native test passed on Aetheri's headless test compositor with an isolated
0.1.19 SDK built from `874ebfd6afb5f4f95f3e047ea8219ec251b17a8a`, including
independent AT-SPI and its disabled-bridge negative. The test copy's pin was
adjusted only for the candidate experiment; actual owner adoption, physical
and human acceptance remain open. See [candidate evidence](aetheri-adoption-assessment.md#aetheri-owned-settings-on-the-aetheri-test-compositor-2026-11-06).

B03/I01/A02/A10/Q06: on base `185dd6947e61c4b0a7180248d9d03718e6286ea1`,
the Aetheri-owned Settings keyboard workflow has simultaneous independent AT-SPI
focus/checked-state checks against 0.1.19. Native event order, a human
screenreader and Aetheri's own compositor remain open; see
[candidate evidence](aetheri-adoption-assessment.md#aetheri-settings-keyboard-and-native-focus-together-2026-11-06).

B03/I01/A10/Q04: on NexaUI base `61dab137b03da4755e15d5beb7c4706d19189e13`,
the unchanged Aetheri-owned Settings host passed a private Weston virtual-keyboard
workflow including 200% text and German against 0.1.19. Aetheri's own compositor,
version decision, physical input and human acceptance remain open; see
[candidate evidence](aetheri-adoption-assessment.md#aetheri-owned-settings-native-keyboard-on-0119-2026-11-06).

B03/L02/A02/A10/Q04: Aetheri's own Settings presentation at
`d242c7abf944e08f2693de0e1848f542be107981` passed isolated scene and native
AT-SPI tests against the installed 0.1.19 candidate based on
`4f8c3426fc56f1d0547aafe8ed5dd2ca37e50025`. Aetheri still pins 0.1.18;
its real owner/compositor, hardware and human acceptance remain open. See
[candidate evidence](aetheri-adoption-assessment.md#aetheri-owned-settings-on-the-0119-candidate-2026-11-06).

L01/D02/A10: on base `6ced584aa23aec468bf6ed607228d3b0a5ac2045`, the
four-button example dock allocates measured label widths and clips drawing;
320px English/German scene checks cover three fonts. Larger text scale, actual
shell integration and human acceptance remain open; see [candidate evidence](aetheri-adoption-assessment.md#measured-four-button-desktop-dock-layout-2026-11-06).

D05/I01/A02: on base `30e05549b17329d2d8007e57069026cc938fbecb`, the
desktop preview and layer keyboard callback exercise Ctrl-Tab focus traversal
between Search and the Files dock action while retaining plain-Tab result
navigation. Actual Aetheri private input, discoverability and human acceptance
remain open; see [candidate evidence](aetheri-adoption-assessment.md#keyboard-focus-between-desktop-search-and-files-2026-11-06).

D05/A02/W05: on base `101507414e0e7c3806e0c1e93ce08dee83b20e9a`, an
independent AT-SPI client exercises Files actions and search through the layer
test host with three simulated views and a removed accessible. Actual Aetheri
shell/owner, physical outputs and human accessibility remain open. See
[candidate evidence](aetheri-adoption-assessment.md#files-actions-through-the-layer-test-hosts-accessibility-loop-2026-11-06).

D05/L01/A02: on base `96e8e06e67e5d95a47758276615c05b660ea30ff`, the
desktop consumer exposes a Files dock intent and the private test host sends
the model's `F` selector; real owner/shell integration remains open. See
[candidate evidence](aetheri-adoption-assessment.md#files-dock-intent-and-owner-protocol-2026-11-06).

D05/L05/A02: on base `dcb3f4ddb20222120f1f557131a573b0c996fe10`, the
external desktop search view exposes native single-selection actions and
returns selection intents for owner validation. The current Aetheri model's Files
result was tested in an independent snapshot; actual shell integration and a
Files dock button remain open. See [candidate evidence](aetheri-adoption-assessment.md#desktop-search-native-selection-owner-2026-11-06).

I01/L05/A02: on base `4cc5833e84cea2b9d19f0032e3d1947f3d152d6a`, queued
Tab/End and Shift-Tab/End sequences in the Files consumer are routed after
the preceding focus change; private Weston and independent AT-SPI checks pass.
Other event orderings and actual Aetheri remain open. See [candidate evidence](aetheri-adoption-assessment.md#files-queued-focus-and-navigation-ordering-2026-11-06).

I01/L05/A02: on base `471348c1b8fee3aa0d75eb129ba1059acc2a1c90`, the
original-model Files consumer limits list keyboard navigation to the list's
focus and verifies clear/close focus with private Weston virtual input. Actual
Aetheri and human assistive review remain open; see [candidate evidence](aetheri-adoption-assessment.md#files-navigation-respects-control-focus-2026-11-06).

P02/L05/A05: on base `8632bfbdef912fed11445e5391b148523e709968`, an
independent AT-SPI client checks bounded visible rows and stable selection and
references through scroll and filter of a synthetic 100,000-key tree. Actual
Aetheri, offscreen navigation and human accessibility acceptance remain open;
see [candidate evidence](aetheri-adoption-assessment.md#native-accessibility-of-a-virtual-100000-row-tree-2026-11-06).

P02/L05: on base `735031712ce53bb3ca4e41e969679337ff9adccc`, the synthetic
workload additionally scans 100,000 actual in-memory names, rebuilds the
50,000 matches and retains the anchor's exact semantic name. Filesystem I/O,
native accessibility and Aetheri's model limit remain open; see
[candidate evidence](aetheri-adoption-assessment.md#named-100000-row-filter-workload-2026-11-05).

P02/L05: a synthetic 100,000-key virtual-list workload on base
`f9ec0de9f98374077e4a1d1fdec3267ebbcef23d` measures index refresh, reorder,
filtered rebuild and bounded visible semantics. Real model filtering, native
accessibility and Aetheri's 4,096-entry model limit remain separate; see
[candidate evidence](aetheri-adoption-assessment.md#synthetic-100000-row-virtual-list-workload-2026-11-05).

T07/A05: on base `4f8c3426fc56f1d0547aafe8ed5dd2ca37e50025`, an
independent AT-SPI client replayed two sequential full terminal-grid updates,
including chunked insertions and deletions. Throughput, stalled clients, real
Aetheri and human acceptance remain open; see
[candidate evidence](aetheri-adoption-assessment.md#large-terminal-snapshot-update-events-2026-11-05).

T07/A05/P03: the 0.1.19 candidate based on
`c3fdd31ae945626b40225e2464421bff24b98417` permits bounded 512-KiB
read-only snapshots while keeping writable requests at 64 KiB. A full 240x100
four-byte-character terminal model produced 96,099 accessible bytes read by
an independent AT-SPI client. Rapid updates, real Aetheri, human and other
platform acceptance remain open; see [candidate evidence](aetheri-adoption-assessment.md#large-unicode-terminal-accessibility-snapshot-2026-11-05).

D05/W05: the private host on base `70a6a91b4f8edfb176f9f6147ab2f63a4ef8f44c`
reports additional Wayland output globals as unmapped without stopping other
views. It does not re-add them; reliable identity and actual Aetheri hotplug
remain open. See [candidate evidence](aetheri-adoption-assessment.md#unmapped-output-fallback-in-the-private-test-host-2026-11-05).

D05/W05/P05: the private test host based on
`c3dc49068f54a969edf78473341591fabf4894c6` defers known output retirement,
clears its focus/requests and accessibility tree and leaves the other views
usable in simulated and controlled-wire tests. Real hotplug, re-addition and
Aetheri ownership remain open; see [candidate evidence](aetheri-adoption-assessment.md#deferred-private-host-output-retirement-2026-11-05).

L10/A02/I01: on base `504943f7be51d9b5d0d4ec869f2ccc4162d04e6f`, the
native nested-keyboard fixture draws font-backed labels and an independent
AT-SPI client checks blocked/enabled/focused states during the same workflow.
Human visual and assistive review and real Aetheri acceptance remain open;
see the [candidate evidence](aetheri-adoption-assessment.md#labeled-nested-dialog-with-independent-at-spi-2026-11-05).

L10/I01: on base `9228a2306f46c91bc7eb97dbdfb0593ba8c709ee`, virtual
keyboard input opened a modal and nested menu, activated a menu item and
restored focus through both levels on private Weston. The fixture lacks drawn
labels and native semantics; human and actual Aetheri acceptance remain open.
See the [candidate evidence](aetheri-adoption-assessment.md#native-keyboard-for-a-nested-modal-and-menu-2026-11-05).

T05/I02: on base `b2a29e28823478789693b896f87c9f89b52be988`, private
Weston simple-IME preedit and invalid Unicode commit reached the real numeric
widget without changing its committed value. In that fixture Ctrl+Z arrived as
text `z` rather than undo; other providers and actual Aetheri remain open. See
the [candidate evidence](aetheri-adoption-assessment.md#numeric-field-with-weston-simple-ime-2026-11-05).

T05/I05: on base `70380daf3e73ffdaa89a5ab34434236ca54d8b24`, an actual
numeric widget exchanged clipboard data with independent wl-clipboard processes
under private Weston and committed only the valid draft. Real Aetheri peers and
human/physical acceptance remain open; see the
[candidate evidence](aetheri-adoption-assessment.md#numeric-clipboard-with-independent-wayland-peers-2026-11-05).

T05/I05: a controlled Wayland numeric paste based on
`4ed4db364203c8ca5a82ce25a9621f565c7ded64` rejects its delayed result when
the selection changes and is then restored. Independent desktop peers and
actual Aetheri acceptance remain open; see the
[candidate evidence](aetheri-adoption-assessment.md#numeric-clipboard-selection-reversion-2026-11-05).

L01/L02/A10/Q06: the full Settings example based on
`71b71fa9b7065ec9e49e9afeea69374e8fe0c8ac` reuses the original-model
consumer's example-only text wrapper for small windows. Automated scene,
native virtual-keyboard, simple-IME and installed-consumer AT-SPI checks passed;
200% scaling, human assistive review and actual Aetheri adoption remain open.
See the [candidate evidence](aetheri-adoption-assessment.md#full-settings-example-small-window-text-reflow-2026-11-05).

W04/L01/A10: the original-model Settings consumer based on
`84ec8384ef30f36f3c2e2945e46e88d52ea46d63` lays out and follows keyboard focus
at 320–480 by 240 logical pixels in controlled German 200% tests with three
DejaVu fonts. Its earlier host stopped updating below 480 by 380. A private
Sway test now resizes to 320x240 and back with an independent AT-SPI client,
which changes and verifies the German high-contrast model at 200%/150%.
At 100% text a private Sway virtual keyboard also changed settings at 320x240,
retained focus through restore and closed with Escape. Other keyboard layouts,
physical input, smaller windows, other fonts/locales and human acceptance remain open.
On base `e71ee304f1e6c23fa562d9e4b11f7e8e12152079`, the small layout extends
to 128x150 with grapheme-boundary emergency wrapping and scrollable controls.
Independent AT-SPI and virtual keyboard resize checks passed at that size on
private Sway. On base `5114c345c7833b303087b7d3754b0e63f5c23b5f`, separated
radio and display labels also forward pointer clicks without extra tab stops;
the radio label was exercised using a private Sway virtual pointer. Still
smaller sizes, long-content performance, real IMEs and human review remain open.
On base `d74583a03cb497b21cb616a421b3d848f8e8fd05`, the consumer reuses
unchanged bounded layouts. A 16-display, long-name small-window benchmark
showed lower repeated frame-construction time on one WSL profile; cold reflow,
presentation, other targets and an approved performance budget remain open.
On base `c075b03a8405c11898cc7cbb081c61260e768884`, single unbroken ASCII
names additionally reuse initial shaped caret positions for emergency wrapping;
the verified fallback still handles other text. The synthetic cold-frame
measurement improved, but cold shaping, other scripts and target budgets remain open.

D07/L09/L11/P07: the [rename review experiment](../../examples/25_aetheri_rename/README.md)
on NexaUI base `28827b139e2d407305a1ff5ac4a41c39baf3720c` borrows Aetheri's
original operation at `8bfa5684c809899edfc2765e6c03478e2629e9bf`. A bounded
headless view distinguishes explicit confirmation, conflict and unknown,
retains the original operation across view replacement and disables replay.
Native modal/assistive acceptance, long-name scrolling, restart recovery and
actual Aetheri integration remain open.
On base `8e9e0cc4d4b6296fd0757e327f5f5c7ef8daea53`, that view uses a modal
scope with tested background blocking, focus traversal and restoration. An
app-owned in-memory latch preserves a confirmation request across view
replacement, including before dispatch. Native and real-service gates remain
open (L09/L10/I07/P07).
On base `55eca9d05604d31dbf81272bfd0952c728709967`, a private Weston window
and independent AT-SPI client exercised modal blocking, explicit confirmation,
an injected unknown result, view/bridge replacement and disabled replay.
The host is a fixed-size polling fixture; native keyboard, real async service,
physical/human acceptance and Aetheri integration remain open (A02/L09/P07).
On base `21abd07873507fa6253a39e4a2b3179f0c45c03f`, virtual-keyboard tests
on private Weston exercised Escape cancellation, Tab/Enter confirmation and
keyboard access to Cancel after an unknown result. Other layouts, physical
input, human acceptance and actual Aetheri integration remain open (I01/L09).
On base `220611c24ce92e4cb6897382681148db4dca91f1`, the rename review uses a
scrollable, byte-preserving preview with confirmation gated until its bottom is
reached. A 255-byte name was navigated by virtual keyboard and exposed in full
to independent AT-SPI; human reading, screenreader scrolling, other fonts and
actual Aetheri integration remain open (T01/L09/A04/P07).
On base `7cf7062d81fbaa33d36b5127a711e65855e48b9b`, explicit previous/next
page buttons let an independent native AT-SPI client navigate the long rename
preview and complete the tested review. At that base, generic scrollbar
increment/decrement actions remained unhandled by the core request queue (A04/L04/L09).
On base `41c7dc2c122c938715a05ea13655dc7359375ef0`, the core accepts those
actions for actual scroll thumbs, revalidates owner/axis/modal state and applies
bounded steps. An independent AT-SPI client exercised both directions on a
vertical thumb. On base `93384d478c27b54a1940023546c1f686035f7ef6`, an
independent AT-SPI client also stepped both axes of the same area independently
on private Weston. Other platform and human assistive acceptance remain open.

L05/L06/I04/P07: the [Files identity experiment](../../examples/24_aetheri_files_identity/README.md)
on base `358965e12779364125f28c3fa54005b0d4a24139` reuses the original Aetheri
Files model at `87ff701f7d793b825c1d9012fc7d4bbd7b30773d` and the installed
virtual-list API. A held parent/file witness and literal name protect a staged
intent against selected-index retargeting after filter, refresh, directory or
entry replacement; the caller must separately invalidate a rejected full query.
This is a controlled owner-side experiment, not a Files UI, grant, durable
identity, actual Aetheri adoption or the P02 100,000-row performance proof.
On base `31da772f94a2fdea863157bf06819a30aa13480b`, a bounded font-backed
headless tree scene adds virtual visible rows, selection retention and direct
keyboard/pointer tests with core semantics. Native window/assistive acceptance,
full focus restoration, persistent identity and P02 remain open.
On base `ef020881b4c2b12f52590388c3405a9472802033`, a native Wayland probe
exercises keyboard, pointer, wheel and refresh on private Weston; an independent AT-SPI
client selects a visible row and observes the original model change. It is
read-only and leaves full Files controls, offscreen semantics, actual Aetheri
acceptance and P02 open.
On base `00bb33d72b6938d6dbfb897911387ea6e0be46b6`, the same probe adds an
editable filter with rejected-query latching, independent AT-SPI editing and
one private simple-IME preedit/commit check. A conservative same-frame scalar
guard can reject valid near-limit replacements; complex input batching,
active-IME shortcuts and real Aetheri acceptance remain open.
On base `992c31f67bea9d6b4ccb857705c24f9b0218a9c3`, restricted focused
text/deletion batches are replayed through the existing text buffer: tested
valid replacements at the byte limit are accepted, while a transient overflow
remains rejected. Complex clipboard, IME, accessibility mutation and arbitrary
event ordering remain separate evidence gaps.
On base `839f80a7e27e64e7f6f2dd28dd2b8f71fe0d6b34`, a private Weston
simple-IME trace showed that Ctrl+Q can arrive as a `q` text commit without
the Q key reaching the client. The native regression distinguishes that path
from a forwarded shortcut; other providers and reliable user-facing close
paths with active composition remain open (I02/T04).
On base `84c3256043bc627bb8cc8c3f7b9935a4fcb5e1d1`, the Files probe has an
owner-handled close button: private Weston pointer, ordinary keyboard and
independent AT-SPI activation were exercised. In a simple-IME attempt, Tab
and Enter were consumed before reaching the client, so keyboard-only close
under that provider remains open (I02/T04/A02/P05).
On base `fd5174b14241b55522af22d5a4cbf3a243b11d48`, empty folders, no
matches and rejected filters have distinct visible and semantic status text.
Rejected filters hide retained-query rows and block hidden-selection keyboard
navigation; independent AT-SPI clients checked two status paths. Live
announcements, localization, human review and Aetheri adoption remain open.
On base `1edde529243be3481245e7b162319a44f9d7f16d`, an independent libatspi
client observed the no-match status insertion and the same object's name
change to the rejected-filter message. Actual screenreader announcements and
human task acceptance remain open (A03/A07).
On base `aa5b5cf6d1a92b4142b0ca353f1244b3a92c3d26`, a Clear filter button
restores the accepted empty query and returns scene focus to the field. Native
keyboard, independent AT-SPI and pointer input with simple-IME were exercised;
clearing uncommitted composition and keyboard routing when Tab is consumed
remain open (I01/I02/T04/A02).
On base `d07ff625ea726de55f66b3c3a6023954ddeaedae`, Clear explicitly resets
the native IME before replacing the filter and reenables it for the refocused
field. A private Weston simple-IME test now rejects a pre-clear composition
and confirms a fresh one works; other input methods, text-input-v3 and delayed
replies remain open (T04/I02/P05).
On base `41563c9fd5aeba89146004430a5e4926c0fbfacf`, programmatic filter
replacement and Ctrl+L's shared clear path also reset the native composition.
A private test-only signal exercises that function during simple-IME preedit;
it does not demonstrate Ctrl+L delivery by a provider that consumes it.

W10/I07/P04: the candidate based on `1b8020e694236d3a2151345ea07064040b9e87fa`
adds optional writable interest to Linux single-window and Wayland group
waits. Controlled nonblocking socket tests exercise backpressure and wakeup;
the native terminal IME test queues one validated commit for its original
Aetheri PTY. These are bounded examples, not evidence of production PTY
backpressure, session switching, or Aetheri acceptance.
On base `00649d379c10bc4565daaf63e6faad8db275e2f2`, the same example queue is
also exercised against a full Linux PTY and a delayed raw-mode slave reader.
Its ordered multi-item transfer, bounded rejection and injected short/error
writes are controlled tests, not an Aetheri shell under actual backpressure.
On base `8c77c8bab7676ff7213d1b23f6ef3edf7cc9fb56`, a private-Weston
consumer additionally fills the PTY while the original shell is stopped,
then resumes it and observes two queued commands and clean exit. Production
session transitions and native Aetheri acceptance remain open.
On base `5c844d726f268b465846b5f8b291ab2c75e7e227`, the independent native
clipboard review/PTY example also uses that queue and passes acceptance and
cancellation checks, including a full PTY before confirmation. Its real-host
and session-policy gates are unchanged.

T07/T08/A05: the [terminal model consumer](../../examples/23_aetheri_terminal_view/README.md)
uses Aetheri model revision `87996e2f12128b172a80b325ad3b539df6aee527`.
The 0.1.15 snapshot extension was developed on NexaUI base
`4763bdd73143708207e678e5df204bc375d98ad6`.
It demonstrates bounded visible cells, owner selection and copy, and simulated
host AT-SPI text/selection. Scrollback without a visible caret remains readable
with caret offset -1 and no selection. Native interaction, editor, PTY/session and other packet 5
gates remain open; this example does not establish Aetheri adoption.
On base `3f93c535fd3bfa9387a232a732594210f3cd6dfd`, the example additionally
routes synthetic queued pointer and selection-key events to the original model,
with revision/focus/cancellation checks and the model's own copy operation.
Native input and independent clipboard peers remain unproved.
I05/I07/T06/T07: on base `aecd753658edf0b0db379eee16a1e527d45f59f2`,
the same consumer combines the installed nonblocking Wayland clipboard transport
with Aetheri's original paste validator and preview. Controlled protocol and
stale-focus evidence is in the example; independent peers, review UI, PTY
delivery and native Aetheri acceptance remain open.
On base `3b4daee263a485639ea313a59853d97d252060bc`, a fixed-size standalone
review scene renders the original escaped preview with modal keyboard controls,
and an independent AT-SPI client pages, cancels and confirms on a simulated
host. Real Aetheri window/input, adaptive layout, localization, independent
clipboard peer, PTY delivery and human accessibility acceptance remain open.
The controlled Wayland fixture also exercises receipt through modal confirmation
and cancellation; it does not substitute for an independent clipboard peer.
A10/D02: on base `14462a33126acbde336151bfe921be7cf4024dd5`, the standalone
paste review supports German/English reflow and 16–32 px text. A 480x380 German
200% headless/AT-SPI case, focus preservation and resize activation guards are
recorded in the example. Actual native resize, physical scale, other locales,
fonts and human assistive acceptance remain open.
A10/P03: on base `b009c9f25205845c43e250707061fb0c6090c4d5`, the example
preflights every preview row before committing a new layout and keeps the
existing paste and scene after a rejected resize. The example records the
later-page wide-glyph counterexample and one bounded local timing; native resize
handling and target-device performance budgets are still open.
I05/I07/T06/T07: on base `782ba7d210fe960af2bd391c2143aaa629fbca4a`, an
independent wl-clipboard 2.2.1 owner and native NexaUI test consumer exercised
Wayland paste, model validation, review cancellation/acceptance, stalled-owner
cancellation and rejected control bytes on private Weston. Copy in the opposite
direction, actual Aetheri/PTY integration and physical acceptance remain open.
I05/I07/T07: on base `ad4a54fcaa01083ec26c228e64d78b0bc2ea5c25`, the terminal
consumer copied the original model's multiline Unicode selection to an
independent Wayland receiver and left an external selection intact for empty
copy on private Weston. Real Aetheri session/PTY, clipboard persistence and
physical acceptance remain open.

T04/T06, I02 and D05: the private desktop test host candidate based on
`340ac8b58c364bc479471e05292b313e9367f857` optionally binds text-input-v3 on
its existing connection and uses the field revision, preedit, surrounding text
and caret contracts. Its controlled wire and callback evidence and remaining
native Aetheri/provider, device, accessibility and physical gates are recorded
in `examples/22_aetheri_desktop_view/README.md`. These rows remain open.

I04/I07/P05: the private test host candidate based on
`bf1080d889a6d82471b28887b0321f6a3bccc9b6` delays pointer release behind a
Wayland sync and cancels it on observed device/view loss or model change. The
controlled server and callback evidence in the desktop example does not rerun
the earlier Aetheri held-device failure and cannot distinguish a real release
from a synthesized release followed by a later removal. The gate remains open.

A01-A05/D05: the candidate based on
`3c5eb737e0432ff4dff4800b295f40365b110dbb` adds a Linux AT-SPI bridge for
host-owned surfaces, reusing the window adapter's snapshots and guarded action
queue. The separate desktop model consumer can be operated by an independent
AT-SPI client using simulated focus/geometry. Actual private Aetheri surfaces,
native focus, screen coordinates, other accessibility workflows and human
screenreader acceptance remain open.

A01-A04/D05: a later candidate based on
`dd455a82b6306940d28823c89b9d8d7d8fbca625` exercises the actual private host's
accessibility polling with three simulated views, distinct focus and removal;
the independent client and negative controls are documented in the desktop
example. This does not prove native output removal or an Aetheri session.

The T05 and A04 rows below were reconciled on 2026-11-05 against published
source `28b60f2fcad76374d5f2a91c722e8a2c27aa5829`. Their individual test results
and earlier source revisions remain attributed in the adoption assessment;
this update does not revalidate the remaining catalogue rows.

The catalogue inspected for the following T05/A03/A04 reconciliation on 2026-11-05
has SHA-256 `d270de71c116e364f6ee949273c05f37a7215d3d9d2d813297a490059b0623d5`.
The implementation and native evidence cited in those rows refer to their own
recorded revisions, up to main source `08f5c1c2b0606a6fb2fcd9cf4f6a14cc886bf2f9`;
this limited update does not claim a new review or acceptance of the other IDs.

B03/B04, I01/I03, A01/A02, D01/D02 and P04/P05 have additional limited
original-model consumer evidence in the [adoption assessment](aetheri-adoption-assessment.md#external-aetheri-settings-model-experiment-2026-11-05).
It does not close the remaining consumer and acceptance gates in those rows.

The catalogue inspected on 2026-11-05 for the cross-workflow reconciliation
below has SHA-256 `d3bb554059c29ba541e1b0a00b16414a4296c629ef2e8a87c0760d67cb11a6b7`;
the selected NexaUI source is `1c57551fae8bbe66500f99311998944941551d7b`.
The crosswalk connects the new cases to existing rows; individual rows still
need a complete current-candidate review. Historical evidence keeps its own
source revision and is not a new execution on this candidate.

Catalog source: `D:/NexaUI/NEXT_AETHERI_UI.md`, SHA-256 `e177c65c3fdd9438a4feb19656f293d990ac68a47a79c0d4af9fc5ae8727be56`;
base revision `05e28bb16c1e60ac339cd9428ff2d3afbafc21f6` with the uncommitted
candidate described in [the adoption assessment](aetheri-adoption-assessment.md).
The catalog is external to this checkout and is not a generated or installed SDK artifact.

The updated catalogue inspected on 2026-11-05 has SHA-256
`4e75bfc2c040c2eae74dfd164ec3ec33482af0b87671237470d4f8e7a13a808e`.
Its 103 IDs were compared with this map on source
`8d82652c08684c792ed36b28ed207dde3236a345`; D11 was the only missing ID and
is added below. This ID reconciliation does not itself review every updated
acceptance criterion or the new concrete integration cases.

This is a **preliminary source map**, not a reviewed acceptance matrix. A source
pointer identifies a relevant contract, implementation or test for investigation;
it does not prove that the full row is implemented or passed. The final column
copies the original observable acceptance target, not a test result. All rows
remain open. “Evidence missing” also covers rows whose implementation still
needs assessment; it must not be interpreted as an implementation-complete claim.
The documented G restrictions are identified as implementation gaps except B01,
whose candidate is implemented but still has unclosed matrix/consumer gates.
Where applicable, external owners are part of the remaining acceptance, not an
excuse to stop independent SDK work.

Existing executed results and their exact boundaries are recorded in the adoption
assessment; tests named here are source locations, not claims they ran for this
candidate. The first-view sequence remains the order in the catalog.

The bold scope text below reproduces the original catalog, including its
baseline-time descriptions; it is not a statement that source line numbers or
restrictions remain unchanged in the current candidate.

Later I05/T06/W10 evidence and the still-open synchronous clipboard wait are
recorded in the [AT-SPI text assessment](aetheri-atspi-text-assessment.md).
The test of a real one-shot owner is intermittent under private Weston and is
not an acceptance pass for the current candidate.
The subsequent Settings keyboard and AT-SPI paste paths are poll-driven on
Wayland; custom callbacks and X11 paste still have synchronous paths. The
subsequent Wayland file-drop reader is also poll-driven, with a separate-source
copy-drag test on private Weston. Actual file-manager and additional compositor
acceptance, as well as the bounded-queue behavior, still need assessment.
The [W04 scaling assessment](aetheri-wayland-scaling-assessment.md) records the
fractional protocol implementation, controlled checks and a headless Sway
software-Vulkan run. Physical monitor, hardware renderer, mixed-output, input
and visual acceptance remain open. An initial-geometry warning observed under
Sway no longer appeared after the correction described in that assessment.
The [adoption assessment](aetheri-adoption-assessment.md) records a T04 v1
keysym-modifier fix, real `weston-simple-im` compose and focus-loss checks,
an AT-SPI control-focus check and serial-race correction. The later four-case
simple-IME workflow passed in an isolated virtual-input fixture; installed-IME
and human acceptance remain open.
For B04, `nx_get_version` reports the linked core's three-part version,
`nx_get_build_features` identifies compiled platform/transport support, and
`nx_get_source_revision` exposes an optional builder-declared source ID.
Independently verified provenance, live session capabilities and a complete
compatibility/error contract remain open.

R01/R03/W04: `nx_headless_render_scaled` now provides scaled logical-command
rasterization into caller-owned RGBA8 memory, reusing the existing software
renderer. The reference parity test covers all command families at 2x, simple
geometry at 0.125x/1.25x/1.5x/8x, and invalid/overflow atomicity. The installed
C17 package consumer also exercises the new symbol. The Linux Wayland
`nx_software_view_*` API now adds bounded native shared-memory presentation;
the Settings example can select it at build time without Vulkan. Controlled
buffer/scale tests and private-Weston keyboard, AT-SPI and simple-IME checks
cover this candidate. The Settings host now falls back on an explicit Vulkan
unsupported result after safe retirement. A Vulkan-linked executable still
requires its loader at startup; mixed-output and complete CPU/GPU parity remain
open.

The version-query candidate based on `e0d9b143a7f6e05b7cea4b07a3e84c679f8dd766`
was checked on Alpine/WSL2 using fresh Wayland-only C17 consumers. Static and
shared installed consumers compared the linked-library result with both their
headers and CMake package version and checked the null-output error. Four
external `add_subdirectory` combinations (static/shared, Vulkan off/on) also
ran; the shared cases verified the resolved library path. The installed-package
probe disabled CMake X11 discovery but did not hide installed X11 files. These
are Linux consumer checks, not Windows/macOS, backend availability or source-pin
acceptance. The pre-1.0 version is unchanged, and the new symbol is absent from
earlier binaries with the same version. Logs are retained locally as
`/tmp/nexaui-version-package-20261105.log` (SHA-256
`5f95e054a167a6c0c8dfaae4f7c65ae80e9d4b493940d0822d4b52233723fe6a`) and
`/tmp/nexaui-version-subdirectory-20261105.log` (SHA-256
`a0f007a7e4ec144672794e559e62c1c854d2b23bc2fe8c15d55205256fe73f21`).

The build-feature candidate based on `038ef30b3b8c627be48795dddf21caa797442252`
was tested on Alpine/WSL2 with Wayland-only shared plus AT-SPI, Wayland-only
static without AT-SPI, and static dual-backend builds. Each reported its
expected bits; asking each test to expect the wrong X11 or AT-SPI state made it
fail. A private protocol-data fixture without fractional-scale-v1 built and
passed the feature and clipboard checks (2/2). Installed static/shared C17
consumers and four static/shared, Vulkan-off/on external `add_subdirectory`
consumers independently checked Wayland present and X11 absent. The focused
feature test also passed with Clang ASan/UBSan. This describes compiled core
facilities, not a live compositor, AT-SPI service, input method or separate
Vulkan renderer. Windows and macOS builds remain untested for this API.

The source-revision candidate based on
`301c238a319a1d6d1565ab303d7d9d6d2752c1f1` was checked on Alpine/WSL2. The
unconfigured case returns unsupported with a cleared pointer. Installed static
and shared C17 consumers compared synthetic 40- and 64-digit declarations
with the linked library and installed CMake metadata, including uppercase to
lowercase normalization. Four invalid or abbreviated inputs were rejected at
configuration. Reconfiguring one build from a synthetic 40-digit to a
64-digit ID rebuilt the core; the old expected value was rejected. The
instrumented text-cost benchmark also built with the generated private header.
These tests establish metadata propagation, not that a declaration matches
the source or dependencies. An existing build cache must be updated explicitly
when the declaration changes. Windows/macOS and independent provenance
verification remain open.

The 0.1.1 development-version candidate based on
`80241dcd1499f03e1ef7587adfc6661d6747e565` makes these additive queries
distinguishable from 0.1.0 at CMake package selection. An installed 0.1.0
shared package from that source was rejected by a consumer requiring 0.1.1; a
new shared installation configured, linked and ran. A loader-path negative
control deliberately resolved the older same-SONAME library and the consumer's
linked-version check rejected it. This older test library already exported the
queries; still older 0.1.0 builds without them may fail symbol resolution
before a comparison. An independent AT-SPI client saw the current toolkit
version under private Weston and rejected the old library in a separate
negative run. Two installed C17 and four add-subdirectory profiles passed.
No Git tag, binary release or 1.0.0 approval is implied; Windows/macOS and
full ABI compatibility remain open.
The tested local old/new shared objects have SHA-256 values
`49d09d0d49b7d96a09b0cd897c56c6aef2ec3406ff58181f02d96ffb2d74629a` and
`ddf3dbe148d681e3a62667c6899356484c12fa8369c9114dbfd0bd19b5563f0b`,
respectively. Their build directories are local test artifacts, not releases.

| ID | Priority | Current disposition | Candidate source / evidence | Observable acceptance still required |
| --- | --- | --- | --- | --- |
| B01 | 0 | X11-free package evidence; matrix incomplete | [build/export](../../CMakeLists.txt), [X11-free root checker](../../tests/check_wayland_no_x11_root.py), [adoption evidence](aetheri-adoption-assessment.md), [add_subdirectory probe](../../tests/check_wayland_only_add_subdirectory.py) | **Explicit Wayland-only Linux build:** Static/shared installed C17 consumers built and ran in an offline Alpine root without standard X11/XCB headers or libraries. Native compositor and default dual-backend acceptance remain separate evidence. |
| B02 | 0 | Partial profile evidence | [dependency profiles](../concepts/dependencies.md), [build/export](../../CMakeLists.txt), [installed Settings probe](../../tests/check_installed_settings.py), [adoption evidence](aetheri-adoption-assessment.md) | **Deliberate software/Vulkan and accessibility dependency profiles:** Static and shared software-only Settings consumers were exercised with AT-SPI explicitly disabled; independently tested Wayland IME and AT-SPI with the transport enabled remain distinct cases. Complete transitive dependency and target-image evidence remains open. |
| B03 | 0 | Linux consumer profiles exercised; acceptance matrix incomplete | [build/export](../../CMakeLists.txt), [package configuration](../../cmake/NexaUIConfig.cmake.in), [installed consumer probe](../../tests/check_wayland_only_package.py), [public Vulkan consumer](../../tests/vulkan_consumer/main.c), [add_subdirectory probe](../../tests/vulkan_subdirectory/CMakeLists.txt) | **Static/shared, add_subdirectory and installed find_package consumers:** Small external consumers build/run with exported targets and no private headers or source-tree paths. Linux Vulkan paths have private-Weston evidence; other platform and selected-compositor gates remain open. |
| B04 | 0 | Version, compiled features and declared revision available; verified provenance, live capabilities and compatibility remain open | [public C API](../../include/nexaui/nexaui.h), [version contract](../api/core.md), [threading](../concepts/threading.md), [allocation/lifecycle](../../src/nexaui.c) | **Exact version, source pin and compatibility/error contract:** Consumer can identify SDK/backend capabilities and handle unsupported features; compatible changes retain existing behavior |
| B05 | 0 | Evidence missing; assess implementation | [public C API](../../include/nexaui/nexaui.h), [threading](../concepts/threading.md), [allocation/lifecycle](../../src/nexaui.c) | **C17 ownership/thread/allocator rules on every public operation:** OOM, invalid input, wrong-thread and close/reentry cases preserve documented state and release owned resources |
| B06 | 1 | Evidence missing; assess implementation | [build/export](../../CMakeLists.txt), [package configuration](../../cmake/NexaUIConfig.cmake.in), [installed consumer probe](../../tests/check_wayland_only_package.py), [add_subdirectory probe](../../tests/check_wayland_only_add_subdirectory.py) | **Optional backend isolation and clean removal:** No hidden downloads or unselected runtime dependencies; installed manifest describes owned files and licenses |
| B07 | 1 | Partial model-view replacement; migration open | [original-model example](../../examples/21_aetheri_model/README.md), [model evidence](aetheri-adoption-assessment.md#external-aetheri-settings-model-experiment-2026-11-05), [threading](../concepts/threading.md) | **Migration without toolkit-owned application persistence:** The app-owned Aetheri appearance/language/size model survives destruction and recreation of this NexaUI view. Actual SDL-to-NexaUI migration, application commands, service state and persistent-data acceptance remain open. |
| B08 | 1 | Evidence missing; assess implementation | [build/export](../../CMakeLists.txt), [package configuration](../../cmake/NexaUIConfig.cmake.in), [installed consumer probe](../../tests/check_wayland_only_package.py), [add_subdirectory probe](../../tests/check_wayland_only_add_subdirectory.py) | **x86-64 Intel/AMD portability and reproducible build inputs:** No unconditional native-CPU flags; generic build/consumer works on the approved baseline, with AM5 acceptance later |
| W01 | 0 | Partial Wayland evidence | [window API](../../include/nexaui/nexaui.h), [window contract](../concepts/window-management.md), [Wayland adapter](../../platform/linux/wayland/window.c), [adoption evidence](aetheri-adoption-assessment.md) | **Real Wayland window/configure/initial-buffer lifecycle:** Installed consumers ran alongside Aetheri's actual SDL desktop on its private headless test compositor. A public probe respected the dock's work area through maximize/restore; Settings rendered, accepted virtual keyboard input and closed. Owner-mediated launch, DRM session and human acceptance remain open. |
| W02 | 0 | Private Weston input and Sway close evidence; acceptance incomplete | [window API](../../include/nexaui/nexaui.h), [window contract](../concepts/window-management.md), [Wayland adapter](../../platform/linux/wayland/window.c), [Wayland two-window host](../../tests/wayland_two_windows.c), [Weston driver](../../tests/run_wayland_two_windows.py), [Sway driver](../../tests/run_sway_two_windows.py), [Windows test](../../tests/test_multi_window.c) | **Multiple independent windows and contexts:** Weston virtual keyboard input switches between windows and survives app-owned close. Headless Sway sends a targeted close request; the other window remains and submits another software frame. The Wayland group wait has bounded idle/worker/input checks; actual Aetheri and human interaction remain open. |
| W03 | 0 | Evidence missing; assess implementation | [window API](../../include/nexaui/nexaui.h), [window contract](../concepts/window-management.md), [Wayland adapter](../../platform/linux/wayland/window.c), [multi-window test](../../tests/test_multi_window.c) | **Move/resize, minimize/maximize/restore and client chrome:** Use observed state/capabilities and valid input serials; refusals and unknown minimized state are not fabricated as success |
| W04 | 0 | Evidence missing; assess implementation | [window scale contract](../concepts/window-management.md), [Wayland adapter](../../platform/linux/wayland/window.c), [text density test](../../tests/test_context_text_density.c) | **Logical size, pixel size, text scale and fractional scaling:** Correct input geometry and crisp rendering at 1x/1.25x/1.5x/2x; no double scaling or stale hit regions |
| W05 | 1 | Partial virtual-compositor observations | [output API](../../include/nexaui/wayland_outputs.h), [output evidence](aetheri-adoption-assessment.md#aetheri-virtual-output-mode-change-while-settings-is-running-2026-11-05), [text density test](../../tests/test_context_text_density.c) | **Up to three outputs: 1080p, 1440p and 4K:** An owner-launched consumer observed three Aetheri headless-compositor outputs and retained selection through a virtual mode change and restore. Mixed-scale movement, real hotplug/removal, physical outputs and human focus acceptance remain open. |
| W06 | 0 | Evidence missing; assess implementation | [window API](../../include/nexaui/nexaui.h), [window contract](../concepts/window-management.md), [Wayland adapter](../../platform/linux/wayland/window.c), [multi-window test](../../tests/test_multi_window.c) | **Focus/activation loss, cancellation and reconnect failures:** Pointer/key capture clears correctly; unavailable activation is reported; display loss does not deadlock or claim restored windows |
| W07 | 1 | Evidence missing; assess implementation | [window API](../../include/nexaui/nexaui.h), [window contract](../concepts/window-management.md), [Wayland adapter](../../platform/linux/wayland/window.c), [multi-window test](../../tests/test_multi_window.c) | **Decorations, fullscreen and resize constraints:** Declared content geometry and controls remain correct across state/decoration changes and compositor-specific limits |
| W08 | 1 | Evidence missing; assess implementation | [window API](../../include/nexaui/nexaui.h), [window contract](../concepts/window-management.md), [Wayland adapter](../../platform/linux/wayland/window.c), [multi-window test](../../tests/test_multi_window.c) | **Cursor shapes and pointer-capture capabilities:** Correct native cursor at each scale; cancellation and unsupported relative/locked pointer paths are explicit |
| W09 | 1 | Evidence missing; assess implementation | [native event contract](../concepts/window-management.md), [Wayland adapter](../../platform/linux/wayland/window.c), [Settings event loop](../../examples/18_settings/main_linux.c) | **Native popup surfaces when in-window clipping is insufficient:** Placement/grabs/serial provenance and teardown follow the host contract; menus do not depend on illegal toplevel positioning |
| W10 | 0 | Partial owner-thread wake contracts | [native event contract](../concepts/window-management.md), [borrowed Wayland host API](../../include/nexaui/wayland_host.h), [Settings event loop](../../examples/18_settings/main_linux.c), [host consumer](../../examples/19_wayland_host/main.c), [multi-window transfer test](../../tests/test_wayland_poll_many_transfer.c) | **Event waiting and redraw deadlines usable by a real app:** Ordinary Wayland windows can wait together with one app descriptor; a host-owned Wayland surface has a separate wait. Controlled tests cover a second window's clipboard deadline and selected reentry/partial-failure cases. The host chooses redraw deadlines; host-owned AT-SPI integration, broader transfer cases and actual Aetheri acceptance remain open. |
| W11 | 1 | Evidence missing; assess implementation | [window API](../../include/nexaui/nexaui.h), [window contract](../concepts/window-management.md), [Wayland adapter](../../platform/linux/wayland/window.c), [multi-window test](../../tests/test_multi_window.c) | **User-session suspend/resume and renderer recovery:** Window/resources recover or report a controlled failure; no stale input, runaway redraw or unauthorized session restoration |
| W12 | 0 | Evidence missing; assess implementation | [window API](../../include/nexaui/nexaui.h), [window contract](../concepts/window-management.md), [Wayland adapter](../../platform/linux/wayland/window.c), [multi-window test](../../tests/test_multi_window.c) | **Preserve other supported platform consumers:** Linux-only changes retain selected Windows/macOS/static/shared contracts; no private native-handle workaround in public app code |
| R01 | 0 | Evidence missing; assess implementation | [rendering](../concepts/rendering.md), [renderer backend contract](../concepts/renderer-backend.md), [Vulkan adapter](../../renderers/vulkan/renderer.c), [headless parity](../../tests/test_headless_parity.c) | **Shapes, clips, rounded geometry, images, glyph masks and layers:** CPU/reference and selected GPU output agree under nested clips and transforms at required scales |
| R02 | 0 | Evidence missing; assess implementation | [rendering](../concepts/rendering.md), [renderer backend contract](../concepts/renderer-backend.md), [Vulkan adapter](../../renderers/vulkan/renderer.c), [headless parity](../../tests/test_headless_parity.c) | **Explicit alpha/color/pixel-format rules:** No halos, premultiplication mismatch or text/image color drift; supported formats and conversion boundaries are documented |
| R03 | 0 | Evidence missing; assess implementation | [rendering](../concepts/rendering.md), [renderer backend contract](../concepts/renderer-backend.md), [Vulkan adapter](../../renderers/vulkan/renderer.c), [headless parity](../../tests/test_headless_parity.c) | **Robust software path and optional GPU path:** VM works without a physical GPU; renderer availability/failure has an explicit capability/fallback result |
| R04 | 1 | Evidence missing; assess implementation | [rendering](../concepts/rendering.md), [renderer backend contract](../concepts/renderer-backend.md), [Vulkan adapter](../../renderers/vulkan/renderer.c), [headless parity](../../tests/test_headless_parity.c) | **Intel HD 620, RTX 2080 Ti and future AMD-host portability:** Separate real driver/device checks; software/lavapipe success is not physical GPU acceptance |
| R05 | 0 | Evidence missing; assess implementation | [rendering](../concepts/rendering.md), [renderer backend contract](../concepts/renderer-backend.md), [Vulkan adapter](../../renderers/vulkan/renderer.c), [headless parity](../../tests/test_headless_parity.c) | **Image/glyph ownership, upload fences and pending-close lifetime:** Borrowed sources are retained/copied as promised; source release, worker completion and renderer close produce no UAF/leak |
| R06 | 1 | Evidence missing; assess implementation | [viewport contract](../concepts/viewport.md), [viewport test](../../tests/test_viewport.c), [renderer backend contract](../concepts/renderer-backend.md) | **External web/PDF/terminal/VM image or GPU content in a UI view:** CPU surface first and reviewed optional GPU interop expose format/stride/ownership/fences; surrounding controls and focus still work |
| R07 | 1 | Evidence missing; assess implementation | [rendering](../concepts/rendering.md), [renderer backend contract](../concepts/renderer-backend.md), [Vulkan adapter](../../renderers/vulkan/renderer.c), [headless parity](../../tests/test_headless_parity.c) | **Offscreen surfaces, thumbnails and cached effects:** Bounded caches invalidate on content/scale/theme changes and retire safely under pending rendering |
| R08 | 2 | Evidence missing; assess implementation | [rendering](../concepts/rendering.md), [renderer backend contract](../concepts/renderer-backend.md), [Vulkan adapter](../../renderers/vulkan/renderer.c), [headless parity](../../tests/test_headless_parity.c) | **Glass/transparency, shadows, gradients and restrained effects:** Correct compositor-supported output plus readable solid fallback; reduced transparency/high contrast remain usable |
| R09 | 1 | Evidence missing; assess implementation | [rendering](../concepts/rendering.md), [renderer backend contract](../concepts/renderer-backend.md), [Vulkan adapter](../../renderers/vulkan/renderer.c), [headless parity](../../tests/test_headless_parity.c) | **Damage/dirty-region and retained-resource reuse:** Unchanged content avoids repeated layout/raster/upload; bounded measurements include actual presentation, not only headless painting |
| R10 | 0 | Evidence missing; assess implementation | [rendering](../concepts/rendering.md), [renderer backend contract](../concepts/renderer-backend.md), [Vulkan adapter](../../renderers/vulkan/renderer.c), [headless parity](../../tests/test_headless_parity.c) | **Device loss, failed allocation, resize and malformed resource inputs:** Defined retry/rebuild or terminal error; no hidden endless retries, corrupt frame or partially adopted resource |
| R11 | 1 | Evidence missing; assess implementation | [viewport contract](../concepts/viewport.md), [viewport test](../../tests/test_viewport.c), [renderer backend contract](../concepts/renderer-backend.md) | **Viewports/plots for simulation content:** Real external simulation frame plus overlay/inspector controls, resize and teardown retain exact ownership |
| R12 | 2 | Evidence missing; assess implementation | [rendering](../concepts/rendering.md), [renderer backend contract](../concepts/renderer-backend.md), [Vulkan adapter](../../renderers/vulkan/renderer.c), [headless parity](../../tests/test_headless_parity.c) | **Export/screenshot/print-friendly render seam:** Requested views render at explicit size/scale without depending on the screen's hidden state; content engines own document encoding |
| T01 | 0 | Evidence missing; assess implementation | [text contract](../concepts/text.md), [public text/window API](../../include/nexaui/nexaui.h), [v1 wire test](../../tests/test_wayland_text_input_v1.c), [v3 wire test](../../tests/test_wayland_text_input_v3.c), [AT-SPI text assessment](aetheri-atspi-text-assessment.md) | **Valid UTF-8 with grapheme-aware cursor/selection/deletion:** Combining characters, umlauts, emoji sequences and malformed-input policy survive typing, copy/paste, selection and undo |
| T02 | 0 | Partial terminal fallback evidence; broader text acceptance open | [text contract](../concepts/text.md), [public text/window API](../../include/nexaui/nexaui.h), [terminal font fallback](../../examples/23_aetheri_terminal_view/README.md#ordered-fallback-for-terminal-and-preedit-glyphs), [AT-SPI text assessment](aetheri-atspi-text-assessment.md) | **Font fallback, shaping, bidi and supported-font contract:** A selected CJK glyph uses an app-provided fallback in terminal cells and preedit, with missing-coverage errors when configured. General corpus, shaping, bidi, cross-font marks and actual target-font acceptance remain open. |
| T03 | 0 | Partial selection and Settings transfer evidence | [text contract](../concepts/text.md), [public text/window API](../../include/nexaui/nexaui.h), [v1 wire test](../../tests/test_wayland_text_input_v1.c), [v3 wire test](../../tests/test_wayland_text_input_v3.c), [AT-SPI text assessment](aetheri-atspi-text-assessment.md) | **Single/multiline text fields and ordinary edit commands:** Selection, word/line movement, scrolling, read-only/disabled modes and bounded undo/redo work through the same input path. Settings transfers grapheme-boundary selection across view replacement; undo and active composition do not transfer. |
| T04 | 0 | Partial private-compositor/IME evidence; target acceptance open | [text contract](../concepts/text.md), [public text/window API](../../include/nexaui/nexaui.h), [v1 wire test](../../tests/test_wayland_text_input_v1.c), [v3 wire test](../../tests/test_wayland_text_input_v3.c), [candidate position](../../examples/23_aetheri_terminal_view/README.md#candidate-position-through-the-native-window), [accessible composition](../../examples/23_aetheri_terminal_view/README.md#read-only-accessible-composition-snapshot), [private Weston IME and PTY](../../examples/23_aetheri_terminal_view/README.md#private-weston-simple-ime-to-the-original-pty), [AT-SPI text assessment](aetheri-atspi-text-assessment.md) | **Wayland IME preedit/commit/surrounding text and candidate rectangle:** A private Weston simple-IME session exercised composed input, PTY delivery and virtual focus cancellation; controlled tests cover v3, guarded state and candidate geometry. Automatic announcement, chosen production IME, real Aetheri focus, production PTY queuing and physical scale acceptance remain open. |
| T05 | 1 | Partial native input and assistive checks; acceptance missing | [text contract](../concepts/text.md), [numeric widget tests](../../tests/test_numeric_field.c), [Windows IME tests](../../tests/test_ime.c), [v1 wire test](../../tests/test_wayland_text_input_v1.c), [v3 wire test](../../tests/test_wayland_text_input_v3.c), [native evidence](aetheri-adoption-assessment.md) | **Complete native text-input path for numeric fields:** Logical and visual commands, opt-in IME and clipboard are implemented. Controlled Wayland v1/v3 tests cover native input and stale batches; Windows controlled composition checks include invalid drafts, undo and focus transfer. Independent Linux AT-SPI and Windows UIA clients exercise numeric drafts and committed values, and macOS AX can replace whole values. Installed IMEs, native selection and composition across platforms, independent clipboard peers, human accessibility and actual Aetheri acceptance remain open. |
| T06 | 0 | Evidence missing; assess implementation | [text contract](../concepts/text.md), [public text/window API](../../include/nexaui/nexaui.h), [v1 wire test](../../tests/test_wayland_text_input_v1.c), [v3 wire test](../../tests/test_wayland_text_input_v3.c), [AT-SPI text assessment](aetheri-atspi-text-assessment.md) | **Clipboard editing and sensitive/password fields:** Unicode transfer works; protected values are not copied into semantics/logs/undo or clipboard by default; unsupported transfer is visible |
| T07 | 1 | Partial visible-cell primitive | [cell-grid contract](../concepts/cell-grid.md), [API](../../include/nexaui/cell_grid.h), [example](../../examples/20_cell_grid/README.md) | **Large editor and terminal text views:** A bounded app-supplied visible grid can be painted without copying history. The example owns keyboard selection and exposes it through a copied read-only Linux AT-SPI snapshot; large editor models, scrolling, native selection mutation and human accessibility acceptance remain open. |
| T08 | 1 | Partial fixed-cell painting | [cell-grid contract](../concepts/cell-grid.md), [API](../../include/nexaui/cell_grid.h), [test](../../tests/test_cell_grid.c) | **Styled spans, fixed-cell metrics, gutters, caret and selection overlays:** Explicit glyph positions, colors, underline and cursor outline exist; full editor spans, gutters, caret/selection interactions and real terminal integration remain open. |
| T09 | 1 | Evidence missing; assess implementation | [text contract](../concepts/text.md), [public text/window API](../../include/nexaui/nexaui.h), [v1 wire test](../../tests/test_wayland_text_input_v1.c), [v3 wire test](../../tests/test_wayland_text_input_v3.c), [AT-SPI text assessment](aetheri-atspi-text-assessment.md) | **Search, selection and autocomplete presentation:** Correct highlighted ranges, popup focus and cancel/accept behavior without corrupting editor/IME state |
| T10 | 1 | Evidence missing; assess implementation | [text contract](../concepts/text.md), [public text/window API](../../include/nexaui/nexaui.h), [v1 wire test](../../tests/test_wayland_text_input_v1.c), [v3 wire test](../../tests/test_wayland_text_input_v3.c), [AT-SPI text assessment](aetheri-atspi-text-assessment.md) | **Text layout, hit testing and caret at mixed density:** Mouse and keyboard target the same grapheme/line through zoom, wrap, font fallback, bidi and monitor migration |
| T11 | 1 | Evidence missing; assess implementation | [text contract](../concepts/text.md), [public text/window API](../../include/nexaui/nexaui.h), [v1 wire test](../../tests/test_wayland_text_input_v1.c), [v3 wire test](../../tests/test_wayland_text_input_v3.c), [AT-SPI text assessment](aetheri-atspi-text-assessment.md) | **Native accessible text/value reading and permitted editing:** Independent AT-SPI client sees actual text/ranges/caret/value changes; stale, readonly, disabled and secret targets reject writes |
| T12 | 2 | Evidence missing; assess implementation | [text contract](../concepts/text.md), [public text/window API](../../include/nexaui/nexaui.h), [v1 wire test](../../tests/test_wayland_text_input_v1.c), [v3 wire test](../../tests/test_wayland_text_input_v3.c), [AT-SPI text assessment](aetheri-atspi-text-assessment.md) | **Terminal/editor inline image and PDF view composition:** Embedded content aligns with text, scroll and selection; supported copying/export and content accessibility have explicit ownership |
| L01 | 0 | Evidence missing; assess implementation | [widget contract](../concepts/widgets.md), [layout contract](../concepts/layout.md), [public controls](../../include/nexaui/nexaui.h), [layout tests](../../tests/test_layout.c) | **Rows, columns, grids, wrapping, overlays and constraints:** Finite/invalid geometry, min/max sizes and narrow/large windows produce usable layout without overlap or NaN propagation |
| L02 | 0 | Partial original-model example; broader acceptance missing | [widget contract](../concepts/widgets.md), [layout contract](../concepts/layout.md), [model view evidence](aetheri-adoption-assessment.md#original-model-view-reflow-2026-11-05) | **Text/control intrinsic measurement and responsive reflow:** The original-model view measures and wraps DE/EN options and scrolls focused controls; a bounded 200% case was tested. Other controls, fonts, sizes and human visual acceptance remain open. |
| L03 | 0 | Evidence missing; assess implementation | [widget contract](../concepts/widgets.md), [layout contract](../concepts/layout.md), [public controls](../../include/nexaui/nexaui.h), [layout tests](../../tests/test_layout.c) | **Buttons, links, checkboxes, switches, radio choices and sliders:** Pointer, keyboard and assistive activation share model/disabled/validation semantics |
| L04 | 0 | Evidence missing; assess implementation | [widget contract](../concepts/widgets.md), [layout contract](../concepts/layout.md), [public controls](../../include/nexaui/nexaui.h), [layout tests](../../tests/test_layout.c) | **Labels, text fields, numeric fields and searchable selection:** Settings forms show units/help/errors and retain focus/draft across validation and asynchronous backend replies |
| L05 | 0 | Evidence missing; assess implementation | [widget contract](../concepts/widgets.md), [layout contract](../concepts/layout.md), [public controls](../../include/nexaui/nexaui.h), [layout tests](../../tests/test_layout.c) | **Lists, tables, headers, sorting and stable selection:** Visible-row virtualization preserves selection/semantic identity when sorted, filtered, refreshed or scrolled |
| L06 | 1 | Evidence missing; assess implementation | [widget contract](../concepts/widgets.md), [layout contract](../concepts/layout.md), [public controls](../../include/nexaui/nexaui.h), [layout tests](../../tests/test_layout.c) | **Trees, breadcrumbs and hierarchical data views:** File/settings/component hierarchy supports keyboard expansion, stable focus and asynchronous child-loading/error states |
| L07 | 1 | Evidence missing; assess implementation | [widget contract](../concepts/widgets.md), [layout contract](../concepts/layout.md), [public controls](../../include/nexaui/nexaui.h), [layout tests](../../tests/test_layout.c) | **Tabs, split panes and docking:** Terminal/editor/Physim panes can resize/reorder/close and restore validated app-owned layouts without losing content state |
| L08 | 0 | Evidence missing; assess implementation | [widget contract](../concepts/widgets.md), [layout contract](../concepts/layout.md), [public controls](../../include/nexaui/nexaui.h), [layout tests](../../tests/test_layout.c) | **Menus, context menus, tooltips and command/search palettes:** Keyboard traversal, pointer capture and focus restoration remain correct across opening, cancellation and action dispatch |
| L09 | 0 | Evidence missing; assess implementation | [widget contract](../concepts/widgets.md), [layout contract](../concepts/layout.md), [public controls](../../include/nexaui/nexaui.h), [layout tests](../../tests/test_layout.c) | **Modal dialogs and ordinary popups:** Background controls are blocked, focus is trapped/restored, close/cancel work and pending app operations are retained |
| L10 | 1 | Nested and sibling scopes; acceptance incomplete | [widget contract](../concepts/widgets.md), [overlay tests](../../tests/test_overlay.c), [searchable-select tests](../../tests/test_search.c), [accessibility evidence](aetheri-adoption-assessment.md) | **Nested overlays, e.g. a selection menu inside a modal:** Up to 16 nested or sibling scopes support topmost focus and dismissal. Independent Linux AT-SPI, Windows UIA and macOS AX clients observe blocked lower actions, invoke active actions and check selected focus-return paths. Broader lifecycle, visual, human screenreader and Aetheri acceptance remain open. |
| L11 | 0 | Partial Settings states | [widget contract](../concepts/widgets.md), [Settings example](../../examples/18_settings/README.md), [scene test](../../examples/18_settings/test_scene.c), [native client](../../examples/18_settings/atspi_unknown_client.c) | **Loading, empty, unavailable, permission-denied, conflict and retry states:** Settings preserves an uncertain operation and draft through an in-memory view replacement without automatic replay; real service reconciliation, durable recovery, permission-denied, conflict and other app states remain open. |
| L12 | 1 | Evidence missing; assess implementation | [widget contract](../concepts/widgets.md), [layout contract](../concepts/layout.md), [public controls](../../include/nexaui/nexaui.h), [layout tests](../../tests/test_layout.c) | **Progress, cancellation, notifications, inspectors and data plots:** Multiple concurrent operations stay distinguishable; unavailable/failed/cancel-pending/complete states remain observable |
| I01 | 0 | Evidence missing; assess implementation | [input contract](../concepts/input.md), [focus contract](../concepts/focus.md), [Wayland adapter](../../platform/linux/wayland/window.c), [Settings keyboard test](../../examples/18_settings/run_keyboard.py) | **Tab/Shift-Tab, directional navigation and visible focus:** All standard controls and dialogs work without a mouse; stable focus survives reflow, filtering and removal |
| I02 | 0 | Evidence missing; assess implementation | [input contract](../concepts/input.md), [focus contract](../concepts/focus.md), [Wayland adapter](../../platform/linux/wayland/window.c), [Settings keyboard test](../../examples/18_settings/run_keyboard.py) | **German/US layouts, key repeat, modifiers and text versus command input:** Native layout changes and IME do not turn text into shortcuts or dispatch actions twice |
| I03 | 0 | Evidence missing; assess implementation | [input contract](../concepts/input.md), [focus contract](../concepts/focus.md), [Wayland adapter](../../platform/linux/wayland/window.c), [Settings keyboard test](../../examples/18_settings/run_keyboard.py) | **Application command dispatch for pointer/key/assistive input:** Equivalent actions reach one app command, with identical permission/validation handling and truthful async state |
| I04 | 1 | Evidence missing; assess implementation | [input contract](../concepts/input.md), [focus contract](../concepts/focus.md), [Wayland adapter](../../platform/linux/wayland/window.c), [Settings keyboard test](../../examples/18_settings/run_keyboard.py) | **Drag/drop including supported MIME/data exchange:** Correct source/target lifetimes, cancellation and explicit import selection; UI does not silently grant filesystem authority |
| I05 | 0 | Partial native clipboard evidence | [input contract](../concepts/input.md), [Wayland async API](../../include/nexaui/wayland_clipboard.h), [cell example](../../examples/20_cell_grid/README.md), [adoption evidence](aetheri-adoption-assessment.md) | **Clipboard and selection ownership/loss:** A bounded application-owned example copies and receives Wayland text; pending paste is canceled on model changes and focus loss. Outgoing requests have a fixed total deadline and bounded concurrency, with incomplete receipt possible. PTY integration, other platforms, closed-source behavior, privacy and human acceptance remain open. |
| I06 | 1 | Evidence missing; assess implementation | [native event contract](../concepts/window-management.md), [Wayland adapter](../../platform/linux/wayland/window.c), [Settings event loop](../../examples/18_settings/main_linux.c) | **Gesture provenance and interaction cancellation:** Move/resize/capture never reuse stale input serials; focus/seat changes cancel the right interaction |
| I07 | 1 | Partial owner-thread wake contracts | [native event contract](../concepts/window-management.md), [borrowed Wayland host API](../../include/nexaui/wayland_host.h), [Settings event loop](../../examples/18_settings/main_linux.c), [host consumer](../../examples/19_wayland_host/main.c) | **Worker/background completion reaching UI:** Bounded examples wake the owner thread through a borrowed descriptor; host callback reentry and fd failure are tested. Real service lifetime, multiple descriptors and actual Aetheri integration remain open. |
| I08 | 1 | Evidence missing; assess implementation | [viewport contract](../concepts/viewport.md), [viewport test](../../tests/test_viewport.c), [renderer backend contract](../concepts/renderer-backend.md) | **Embedded browser/VM/viewport focus routing:** Embedded content receives intended text/pointer input; host controls and an explicit escape route remain usable |
| A01 | 0 | Evidence missing; assess implementation | [accessibility contract](../concepts/accessibility.md), [AT-SPI contract](../concepts/atspi.md), [Linux provider](../../platform/linux/accessibility.c), [AT-SPI text assessment](aetheri-atspi-text-assessment.md) | **Stable semantic IDs, names, roles, bounds and state:** Real composed controls publish the same actions/state seen in the rendered view |
| A02 | 0 | Partial independent AT-SPI evidence | [accessibility contract](../concepts/accessibility.md), [AT-SPI contract](../concepts/atspi.md), [Linux provider](../../platform/linux/accessibility.c), [Aetheri evidence](aetheri-adoption-assessment.md) | **Linux AT-SPI discovery, focus and activation:** An independent client operated installed Settings using a private registry alongside Aetheri's SDL desktop on its headless test compositor; an AT-SPI-disabled build failed discovery. Actual target-image services, shared desktop accessibility and human screenreader acceptance remain open. |
| A03 | 1 | Partial native selection evidence; acceptance missing | [selection contract](../concepts/multiselection.md), [Linux native report](linux-multiselection-report.md), [Windows single-selection report](windows-uia-selection-report.md), [macOS AX selection client](../../tests/macos_multiselection_client.c) | **Single/multiple selection, tabs, lists and tree/table membership:** Existing model-backed selection and independent native clients cover bounded complete sets, list/table owners, native actions, lifetime and revision checks on Linux, Windows and macOS in the cited environments. Virtualized application views, further native roles/events, maximum-size external IPC, physical/human assistive use and actual Aetheri acceptance remain open. |
| A04 | 1 | Partial native text/value/selection evidence | [accessibility contract](../concepts/accessibility.md), [AT-SPI contract](../concepts/atspi.md), [native evidence](aetheri-adoption-assessment.md), [macOS provider](../../platform/macos/accessibility.m), [Windows value client](../../tests/uia_value_client.c) | **Text/value/range/scroll interfaces for editor, numeric and slider controls:** Independent Linux AT-SPI and Windows UIA tests exercise numeric value editing and validation; an independent macOS AX client exercises whole-value replacement, UTF-16 text selection and selection-change notifications, including an unchanged native selection after a prefix edit. Protected and read-only selection behavior is also checked in-process on macOS. Windows native text selection, macOS range replacement/geometry, broader notification cases, complete slider/scroll behavior, human screenreader and application acceptance remain open. |
| A05 | 1 | Partial app-owned read-only text | [AT-SPI contract](../concepts/atspi.md), [grid example](../../examples/20_cell_grid/README.md), [terminal composition](../../examples/23_aetheri_terminal_view/README.md#read-only-accessible-composition-snapshot), [independent client](../../examples/20_cell_grid/atspi_client.c) | **Rich document and terminal accessibility integration:** Bounded terminal text and a separate opt-in uncommitted composition can be queried through Linux AT-SPI Text. Native selection mutation, rich navigation, real focus/announcement, sensitive-input policy, other platforms and human screenreader acceptance remain open. |
| A06 | 0 | Evidence missing; assess implementation | [accessibility contract](../concepts/accessibility.md), [AT-SPI contract](../concepts/atspi.md), [Linux provider](../../platform/linux/accessibility.c), [AT-SPI text assessment](aetheri-atspi-text-assessment.md) | **Disabled, hidden, modal-blocked, stale and destroyed controls:** Native requests reject invalid targets and preserve model state; fresh replacement controls remain usable |
| A07 | 0 | Evidence missing; assess implementation | [accessibility contract](../concepts/accessibility.md), [AT-SPI contract](../concepts/atspi.md), [Linux provider](../../platform/linux/accessibility.c), [AT-SPI text assessment](aetheri-atspi-text-assessment.md) | **Event accuracy and bounded asynchronous provider operations:** No stale-event storm, owner-thread deadlock or indefinite query; actual native failure is reported without disabling the test |
| A08 | 0 | Evidence missing; assess implementation | [accessibility contract](../concepts/accessibility.md), [AT-SPI contract](../concepts/atspi.md), [Linux provider](../../platform/linux/accessibility.c), [AT-SPI text assessment](aetheri-atspi-text-assessment.md) | **Dark/light/high-contrast palettes and visible keyboard cues:** Text and controls meet agreed measured contrast, including selection/error/disabled/focus states |
| A09 | 0 | Evidence missing; assess implementation | [accessibility contract](../concepts/accessibility.md), [AT-SPI contract](../concepts/atspi.md), [Linux provider](../../platform/linux/accessibility.c), [AT-SPI text assessment](aetheri-atspi-text-assessment.md) | **Reduced motion and readable opaque fallback:** Animations can be disabled/interrupted; information/actions remain available without blur/transparency |
| A10 | 1 | Partial model-view evidence | [accessibility contract](../concepts/accessibility.md), [AT-SPI contract](../concepts/atspi.md), [model view evidence](aetheri-adoption-assessment.md#original-model-view-reflow-2026-11-05) | **Text scaling, magnification-friendly reflow and localized labels:** At 480x380 and German 200%, a scene test checks Tab focus rectangles and one unshrunk label. Native 480x380 pixel/input, other windows/fonts, physical scale and human assistive acceptance remain open. |
| D01 | 0 | Evidence missing; assess implementation | [themes](../concepts/themes.md), [animation](../concepts/animation.md), [public API](../../include/nexaui/nexaui.h), [theme test](../../tests/test_theme.c) | **Shared semantic tokens, density, themes and per-view overrides:** Settings/Files/terminal chrome use consistent state/style without hardcoded app-specific colors in core widgets |
| D02 | 0 | Partial example; acceptance missing | [Settings example](../../examples/18_settings/README.md), [scene and local catalog](../../examples/18_settings/scene.c), [scene test](../../examples/18_settings/test_scene.c), [public API](../../include/nexaui/nexaui.h) | **German/English text, pluralization and locale-safe formatting seams:** Labels, errors, units and variable-length translated content update without changing stable command/semantic IDs. The example covers a bounded DE/EN set; general pluralization, formatting and complete acceptance remain open. |
| D03 | 1 | Evidence missing; assess implementation | [themes](../concepts/themes.md), [animation](../concepts/animation.md), [public API](../../include/nexaui/nexaui.h), [theme test](../../tests/test_theme.c) | **Theme/font/scale changes while views and overlays are alive:** Correct invalidation, retained drafts and no stale cached glyph/effect sizes |
| D04 | 2 | Evidence missing; assess implementation | [themes](../concepts/themes.md), [animation](../concepts/animation.md), [public API](../../include/nexaui/nexaui.h), [theme test](../../tests/test_theme.c) | **Interruptible feedback and animation scheduling:** Rapid input reverses/cancels transitions; idle frames stop and reduced motion is honored |
| D05 | 1 | Partial host-owned software presentation | [Wayland host API](../../include/nexaui/wayland_host.h), [window host contract](../concepts/window-management.md), [standalone consumer](../../examples/19_wayland_host/README.md), [controlled role test](../../tests/test_wayland_host.c) | **Shell surface host seam for dock/search/panels/notifications:** SDK views render into a host-selected Wayland role without opening a second connection. A controlled fake role and ordinary xdg consumer cover the software seam; actual Aetheri private roles, input/semantics and multi-output acceptance remain open. Compositor/session/authority remain external owners. |
| D06 | 1 | Evidence missing; assess implementation | [window host contract](../concepts/window-management.md), [accessibility contract](../concepts/accessibility.md); compositor/session/authority are external owners | **Trusted permission/authentication/lock presentation seam:** UI can render approved models without storing secrets or deciding authorization; secure host/session routing cannot be bypassed by ordinary app content |
| D07 | 1 | Evidence missing; assess implementation | [window host contract](../concepts/window-management.md), [accessibility contract](../concepts/accessibility.md); compositor/session/authority are external owners | **File/device/software pickers and progress surfaces:** App-selected models can show scope, retained/shared state and errors; native UI output is not treated as a grant or operation receipt |
| D08 | 1 | Evidence missing; assess implementation | [viewport contract](../concepts/viewport.md), [viewport test](../../tests/test_viewport.c), [renderer backend contract](../concepts/renderer-backend.md) | **External web/PDF/VM content adapter contract:** Focus, IME, size/scale, input, accessibility subtree and renderer lifetime are defined for the selected maintained engine |
| D09 | 1 | Evidence missing; assess implementation | [themes](../concepts/themes.md), [animation](../concepts/animation.md), [public API](../../include/nexaui/nexaui.h), [theme test](../../tests/test_theme.c) | **Icons, cursors, images and licensed theme assets:** Approved formats render at all scales; missing assets have readable fallback and actual license/ownership records |
| D10 | 2 | Evidence missing; assess implementation | [window host contract](../concepts/window-management.md), [accessibility contract](../concepts/accessibility.md); compositor/session/authority are external owners | **Multiple monitor shell views and trusted overlay ordering:** Dock/search/notifications appear on the intended output and do not bypass lock/modal policy; compositor owns global ordering |
| D11 | 1 | Partial host profile; workflow open | [offline profile](../concepts/dependencies.md#minimal-wayland-software-build-for-offline-hosts), [public controls](../../include/nexaui/nexaui.h), [accessibility contract](../concepts/accessibility.md) | **Installation, first-run and recovery:** A bounded software-only host profile is documented and exercised with the Settings model. Target review, real installer progress/errors, recovery authorization, accessible fallback and reconnect-safe operations remain open and owned by the application. |
| P01 | 0 | Evidence missing; assess implementation | [window polling](../concepts/window-management.md), [Settings event loop](../../examples/18_settings/main_linux.c), [idle evidence](aetheri-adoption-assessment.md#settings-idle-scheduling-and-native-keyboard-finding-2026-11-04) | **Event-driven idle for normal and hidden/minimized views:** Measure CPU, wakeups and redraw/present counts; no mandatory constant refresh loop for an unchanged view |
| P02 | 1 | Evidence missing; assess implementation | [virtual scrolling test](../../tests/test_scroll_virtual.c), [table test](../../tests/test_table.c), [tree test](../../tests/test_tree.c) | **Bounded large-list/table/tree UI cost:** Defined 100,000-row corpus uses visible widgets plus bounded indexing/cache; measure refresh/filter/scroll and accessibility behavior |
| P03 | 1 | Evidence missing; assess implementation | [text contract](../concepts/text.md), [text layout](../../src/text_layout.c), [text buffer](../../src/text_buffer.c); large-content adapter not established by these sources | **Large editor/terminal/document view cost:** Declared large-content corpus measures input, selection, scrolling and memory; no unbounded text copy or per-character semantic-node explosion |
| P04 | 0 | Evidence missing; assess implementation | [public C API](../../include/nexaui/nexaui.h), [threading](../concepts/threading.md), [allocation/lifecycle](../../src/nexaui.c) | **Allocator failures, overflow and bounded queue/cache sizes:** Controlled errors preserve state; pending messages, images, glyphs, semantic snapshots and effects have budgets and release paths |
| P05 | 0 | Evidence missing; assess implementation | [public C API](../../include/nexaui/nexaui.h), [threading](../concepts/threading.md), [allocation/lifecycle](../../src/nexaui.c) | **Context/window/resource lifetime and reentrant close safety:** Pending render/input/assistive/worker operations cannot use freed application objects; repeated open/close has stable owned-resource counts |
| P06 | 1 | Evidence missing; assess implementation | [text contract](../concepts/text.md), [font parser](../../src/font.c), [image/raster path](../../src/raster.c) | **Malformed text/font/image and external-content boundaries:** Bounds/fuzz/failure tests exercise the selected parsers; untrusted content cannot select arbitrary host paths or privilege |
| P07 | 1 | Evidence missing; assess implementation | [window polling](../concepts/window-management.md), [Settings event loop](../../examples/18_settings/main_linux.c), [idle evidence](aetheri-adoption-assessment.md#settings-idle-scheduling-and-native-keyboard-finding-2026-11-04) | **Pause/resume and backend/session failure:** UI does not replay an install/delete/VM-reset command because a window, bus or renderer reconnects |
| P08 | 0 | Evidence missing; assess implementation | [public C API](../../include/nexaui/nexaui.h), [threading](../concepts/threading.md), [allocation/lifecycle](../../src/nexaui.c) | **Diagnostics without private data:** Errors/counters identify capability and owner lifecycle without credentials, document text or personal paths in routine telemetry |
| P09 | 1 | Evidence missing; assess implementation | [window polling](../concepts/window-management.md), [Settings event loop](../../examples/18_settings/main_linux.c), [idle evidence](aetheri-adoption-assessment.md#settings-idle-scheduling-and-native-keyboard-finding-2026-11-04) | **Real end-to-end responsiveness and footprint:** Record startup-to-visible-and-operable frame, input-to-present, frame percentiles, RSS/cache growth and idle behavior on selected targets |
| P10 | 2 | Evidence missing; assess implementation | [window polling](../concepts/window-management.md), [Settings event loop](../../examples/18_settings/main_linux.c), [idle evidence](aetheri-adoption-assessment.md#settings-idle-scheduling-and-native-keyboard-finding-2026-11-04) | **Complete independent-app close/removal:** SDK owns no unexplained resident process or persistent user state; resources/caches integrate with app ownership and explicit cleanup |
| Q01 | 0 | Evidence missing; assess implementation | [adoption evidence](aetheri-adoption-assessment.md), [private compositor report](wayland-private-compositor-report.md), [Wayland results checker](../../tests/check_wayland_results.py) | **One exact selected SDK candidate per result:** Source revision, build options/toolchain/dependency versions and actual loaded library match the consumer's artifacts |
| Q02 | 0 | Evidence missing; assess implementation | [adoption evidence](aetheri-adoption-assessment.md), [private compositor report](wayland-private-compositor-report.md), [Wayland results checker](../../tests/check_wayland_results.py) | **Contract/unit/negative cases plus sanitizer coverage:** Real assertions survive Release/NDEBUG; ASan/UBSan and lifecycle failure tests retain original failing evidence |
| Q03 | 0 | Evidence missing; assess implementation | [adoption evidence](aetheri-adoption-assessment.md), [private compositor report](wayland-private-compositor-report.md), [Wayland results checker](../../tests/check_wayland_results.py) | **Native Wayland consumer on a private real compositor:** Required cases use actual Wayland buffers/input/protocols; skipped or missing native cases cannot count as success |
| Q04 | 1 | Partial Aetheri headless consumer evidence | [Aetheri evidence](aetheri-adoption-assessment.md#wayland-application-id-and-aetheri-activation-2026-11-05), [private compositor report](wayland-private-compositor-report.md) | **Native Aetheri/wlroots compatibility:** The original supervisor launched, activated the existing window, closed and reopened a limited original-model consumer on Aetheri's headless test compositor. Full Settings, app admission, private shell-role adapter and DRM/physical acceptance remain open. |
| Q05 | 1 | Partial Linux evidence; matrix incomplete | [build/export](../../CMakeLists.txt), [package configuration](../../cmake/NexaUIConfig.cmake.in), [installed consumer probe](../../tests/check_wayland_only_package.py), [public Vulkan consumer](../../tests/vulkan_consumer/main.c), [add_subdirectory probe](../../tests/vulkan_subdirectory/CMakeLists.txt) | **Static/shared/installed consumers and selected backend matrix:** Consumer APIs, native dependencies and renderer-off profiles work without source-tree or private-handle assumptions. Vulkan-off component rejection and Linux software-rendered native paths are tested; the broader matrix remains open. |
| Q06 | 1 | Evidence missing; assess implementation | [adoption evidence](aetheri-adoption-assessment.md), [private compositor report](wayland-private-compositor-report.md), [Wayland results checker](../../tests/check_wayland_results.py) | **Keyboard, IME, clipboard and assistive acceptance:** Independent native clients and human workflows cover the composed apps, not only isolated widgets |
| Q07 | 2 | Evidence missing; assess implementation | [adoption evidence](aetheri-adoption-assessment.md), [private compositor report](wayland-private-compositor-report.md), [Wayland results checker](../../tests/check_wayland_results.py) | **Physical three-monitor/GPU/input/suspend acceptance:** Laptop and main-PC profiles pass separately; software rendering/WSLg/native CI do not replace hardware proof |
| Q08 | 1 | Evidence missing; assess implementation | [adoption evidence](aetheri-adoption-assessment.md), [private compositor report](wayland-private-compositor-report.md), [Wayland results checker](../../tests/check_wayland_results.py) | **Consumer examples and upgrade/release notes:** Every newly needed contract has a small public C17 example, failure behavior and version/migration notes |

## Concrete integration case crosswalk

These entries refine the existing IDs; they do not add a second backlog or
claim a full pass. The source catalogue contains the complete acceptance text.

| Workflow and IDs | Existing seam / evidence | Required next evidence or external owner |
| --- | --- | --- |
| Ordinary Settings — B03/B04, W01/W03/W04, Q04/Q05 | [installed Settings and original-model runs](aetheri-adoption-assessment.md) on the private Aetheri test compositor; a separate probe checked dock bounds and maximize/restore | Combine those behaviors in the actual owner-launched consumer; record logical/buffer/input/semantic coordinates and actual output lifecycle. |
| Private desktop — D05/D10, W01/W02/W06, R02/R10 | [borrowed Wayland presenter](../../include/nexaui/wayland_host.h) and [host example](../../examples/19_wayland_host/README.md) | Integrate Aetheri's existing privileged connection and role; prove input, semantics, transparent repaint, resize, three outputs and peer-loss behavior. Session authority remains external. |
| Async reply — W10, I07, L04/L11, P01/P05/P07 | [Settings session and worker](../../examples/18_settings/README.md), [multi-window wait](../concepts/window-management.md) | Real service correlation/reconnect, draft and focus preservation, late replies and accessibility deadlines with actual owner lifetime. |
| Terminal grid — T01/T07/T08/T10, A05, P03 | [cell grid consumer](../../examples/20_cell_grid/README.md), bounded read-only semantic snapshot | Actual terminal cells, history/selection, width and offset mappings, resize and measured budgets; PTY and ANSI remain app-owned. |
| Terminal/editor interaction — I02/I03/I05, L07, T03/T04/T09/T12 | [text/input](../concepts/text.md), [clipboard API](../../include/nexaui/wayland_clipboard.h) | Real editor/pane model, DE/US input, preedit versus commit, focus restoration and app-owned whole-paste policy. |
| Composition lifetime — T01/T04/T06/T10, I02, P04/P05 | [Wayland input tests and assessment](aetheri-adoption-assessment.md) | Actual input-context reset, exactly-once commit, candidate geometry, stale identity/address reuse, focus/keymap/close and sensitive-field policy. |
| Clipboard and paste review — I05/I07, T06/T07, W06/W10, P04/P05/P08 | [bounded read API](../../include/nexaui/wayland_clipboard.h), fixed-deadline outgoing transfers in [assessment](aetheri-adoption-assessment.md) | Independent Aetheri peers and slow/partial/late transfers, immutable snapshots, revalidation and app-owned complete paste review; no transfer result proves revocation. |
| Terminal/session close — L09/L11, P05/P07, B07 | [app-owned session example](../../examples/18_settings/README.md) demonstrates a separate view lifetime | Actual PTY/process/channel state and retained session; close or EOF alone must not claim all work drained. Persistence beyond windows is a product decision. |
| Browser/document — R05/R06/R10, D08, I08, A05 | [renderer contract](../concepts/renderer-backend.md), [viewport](../concepts/viewport.md) | Reviewed engine-owned frame contract, focus/IME/accessibility handoff, crash and late-frame behavior; engine stays external. |
| Files/grants/removal — D07, L05/L06/L09-L12, I04, P07 | [widgets](../concepts/widgets.md), [layout](../concepts/layout.md) | Real stable item IDs and independently observed grants/operations, precise retained-data scope and error/unknown states; UI selection grants no authority. |
| VM/experiment console — D07/D08, I08, L09/L11, R06, P07 | [viewport](../concepts/viewport.md) provides a starting surface | Real guest/display adapter, host escape and key release, individual sharing grants and honest reset/external-write scope; guest content is untrusted. |
| Login/lock — D06, T06, A06, P08 | [text](../concepts/text.md) and [accessibility](../concepts/accessibility.md) document partial sensitive-field behavior | Aetheri trusted session enforces routing and lock; test unavailable providers and prevent secrets in diagnostics, semantics and caches. |
| Installer/recovery — D11, B02/B06, R03, P07, Q08 | [offline software profile](../concepts/dependencies.md#minimal-wayland-software-build-for-offline-hosts) | Real offline installer/recovery host, observed operation and target on reconnect, accessible unavailable behavior and non-graphical fallback. |
| Plugins/theme/cleanup — B05/B06/B07, D01/D03/D09, P04/P05/P10 | [theme](../concepts/themes.md), [threading](../concepts/threading.md) | Pending callback/texture retirement, cache invalidation and declared owned files/processes. Plugin trust and app data stay app-owned. |

Performance acceptance additionally requires a fixed corpus, viewport, renderer,
machine and agreed budgets; virtual/software results do not establish physical
GPU or multi-output acceptance.

## Application ownership

The catalog’s thirteen application surfaces are consumer acceptance cases, not
additional claims that an SDK widget already implements an application. Models,
commands, authorization, content engines and compositor roles retain the owners
named in the catalog. Aetheri remains free to use SDL3 until an individual view
passes its own gates; no consumer pin or external project is changed here.

| Surface | Related SDK rows | Consumer or host acceptance still needed |
| --- | --- | --- |
| Desktop shell | W05/W09, L08/L12, D05/D10, I01 | Aetheri shell roles, multi-output ordering and keyboard launch/switch |
| Login, lock and permission | T06, A01/A02/A06, D06/D07 | Trusted session routing, authorization and secret protection |
| Settings and devices | B01–B05, W01/W04/W10, T03–T06, L01–L04/L11, A01/A02/A08/A09 | Real backend state, validation, keyboard, IME and accessibility acceptance |
| Files and trusted pickers | L05/L06/L11/L12, I04/I05, D07, P02 | Real large directory data, transfers, cancellation and authority boundaries |
| Terminal and editor | T07–T12, L07/L08, A04/A05, P03 | App-owned PTY/editor model, large text, accessible content and responsiveness |
| Browser | R06, T04, I08, D08 | Maintained content engine, focus, IME, scale and accessible subtree |
| Image and PDF viewer | R06/R07/R12, D08, A05 | Maintained decoder/document engine, navigation and owned surface lifetimes |
| Software and development tools | L05/L06/L11/L12, D07, P07 | Actual backend progress, permission and retained-data state |
| Experiment and VM management | R06, I08, L11/L12, D07/D08 | Actual guest console, host escape route and authority-backed sharing |
| Component/process overview | L05/L06/L11, P02/P09 | Real owner-scoped snapshots, reasons and refresh behavior |
| Physim | R11, L07, T05, P09 | Actual simulation handoff and measured rendering; migration separately scoped |
| NexaCloud desktop/data | B03, L05/L09/L11/L12, T06, A01/A02 | Existing app design and model preserved with real consumer acceptance |
| Installation, first-run and recovery | D11, B02/B06, R03, P07, Q08 | Offline target review, accessible setup/progress and recovery with truthful operation state and a non-graphical host fallback |
