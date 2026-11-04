# Linux AT-SPI text adoption assessment

Baseline `05e28bb16c1e60ac339cd9428ff2d3afbafc21f6`; current working
candidate has the separate Wayland-only and Settings changes documented in
[aetheri-adoption-assessment.md](aetheri-adoption-assessment.md). This document
is a source assessment, not a completed implementation or native acceptance.

The independent Settings client confirms a limited AT-SPI Text read path and
whole-value, insert and delete EditableText operations, plus caret and selection
movement, for its editable name.
Clipboard operations use the submitted field's callbacks. The provider's snapshot in `platform/linux/accessibility.c`
contains copied semantic name, description, value, bounds, role and state. The
current candidate also copies an internal completed-frame caret/selection snapshot
for genuine non-protected text fields; positions are UTF-8 byte offsets, not
AT-SPI character offsets. The public `nx_text_field_selection` itself is an
owner-thread model query and must not be called from a D-Bus query on a retained
object. The core already offers `nx_semantics_request_text_value` for whole
replacement with UTF-8 validation, owner-thread admission and deferred widget
application. The AT-SPI adapter reuses it and distinguishes admission from
successful application rather than introducing a second editor.

A separate in-progress AppKit worktree has staged changes to the same portable
`src/nexaui.c` and `tests/test_text_value.c` that capture and check the full
ordered ancestor ID/role chain for deferred text requests. Those staged changes
remain separate and have not been copied or overwritten. This candidate now
independently reuses the existing multi-selection ancestry machinery to check
text requests; the overlapping AppKit change still requires reconciliation.
Linux-native requests now have binding/incarnation, disabled/protected and
connection-lifetime guards; modal and reentrancy edge cases still need native
end-to-end coverage.

Required implementation and evidence remain:

1. The private copied snapshot contains byte-based cursor/selection for
   non-protected built-in text fields; native character counts and range reads
   now convert Unicode scalars. Expand native range/identity/size edge coverage
   and expand text/caret/selection notification coverage. Preserve secret suppression,
   stale identities, and snapshot/value coherence.
2. Implement the applicable complete protocol methods and honest unsupported
   results. Avoid advertising Text/EditableText merely because a semantic node
   has a string; custom and protected nodes need explicit policies.
3. Copy and bound native write requests, route them on the owning thread, and
   revalidate current field identity, ancestry, revision, policy and native
   lifetime through the actual widget commit. Whole replacement, insert/delete,
   selection, caret and clipboard have different semantics and must not be
   conflated with a queued success acknowledgement.
4. Independently test a real composed Settings field via libatspi/D-Bus on a
   private registry and compositor: Unicode positions, preedit interaction,
   model acknowledgement, read-only/disabled/protected targets, stale handles,
   detach/reparent/reconnect, allocator failures and bounded queues. Repeat
   static/shared and relevant sanitizer profiles. Installed input methods and
   actual screenreader/human work remain separate acceptance.

`docs/concepts/atspi.md`, `include/nexaui/nexaui.h`, `src/text_value.h`,
`src/nexaui.c` and `platform/linux/accessibility.c` are the revision-specific
source contracts. Existing linux-specialist AT-SPI field notes provide prior
Physim evidence only; they are not NexaUI acceptance.

## Internal selection snapshot (2026-11-04)

`src/text_value.h` defines a private completed-frame value containing byte-based
anchor and cursor and no editor pointer. `src/nexaui.c` captures it after the
field's input/model changes, alongside the semantic value. The Linux provider
copies it into its owned native snapshot before any application allocator
callback. Password fields and arbitrary semantic nodes publish no such metadata.
The core test covers reversed selection across a four-byte Unicode scalar,
completed-frame lifetime after model replacement and field destruction, and
the password/custom-node exclusions. That snapshot step itself did not advertise
AT-SPI Text or EditableText. Later read-only work is described below; native
editing, full protocol coverage and native lifetime guards remain open.

The complete locally registered private-Weston Wayland-only suites passed:
124/124 in the GCC shared build and 125/125 in the Clang static build with
ASan/UBSan and leak checking. Both fixture and child returned 0, and the
sanitizer log has no ASan, LeakSanitizer or UBSan error marker. The shared full
run preceded the addition of explicit semantic-value consistency assertions to
the new test; that strengthened test was then rebuilt and passed separately in
both profiles. The static full run includes those assertions. The shared test's
loader resolved `/tmp/nexaui-b01-shared-v1/libnexaui.so.0`. These are virtual
Weston/Xvfb tests, not actual Aetheri-compositor or human assistive acceptance.
Raw full logs: `out/aetheri-text-snapshot-shared-v1.log` SHA-256
`52558431735b2f063d4f4bd36c7b65bf7011631fa5090abae6b59e330ef73996`, and
`out/aetheri-text-snapshot-asan-v1.log` SHA-256
`27f1752f0768723130664b5c531ce1d0ca10f100ef93edb434d64814aabc7974`.
The strengthened focused runs are in `out/aetheri-text-snapshot-shared-focused-v1.log`
and `out/aetheri-text-snapshot-asan-focused-v1.log`. Existing AppKit changes
remain in their separate worktree and were not incorporated.

A targeted negative control replaced only the snapshot assignment with an
empty snapshot: the strengthened test failed at its `old.present` assertion
(CTest exit 8, `out/aetheri-text-snapshot-negative-v1.log`). The original
source was restored and rebuilt, and the same focused test passed again. This
checks test sensitivity to the new capture; it does not validate native AT-SPI
text queries, which were not implemented at the time of that control.

## Partial native text reading (2026-11-04)

Built-in non-protected text fields now advertise a partial AT-SPI Text interface
only when the owned value and both byte offsets form valid UTF-8 boundaries.
The provider converts byte offsets to Unicode scalar offsets during snapshot
creation; it exposes count, caret, selection, character and range reads and
character-granularity `GetStringAtOffset`. Native text replies are limited to
1 MiB and invalid ranges return an error. Unsupported methods, including
editing, movement, geometry, attributes and other granularities, return
`NotSupported`. At this historical step no EditableText interface was implemented. Text capability, field identity and ancestor
changes retire the old object path, but independent stale-path tests for this
new behavior remain to be added.

An independent libatspi client reads the real Settings draft `Aé🌍`: three
scalar values, the four-byte character, the substring `é🌍`, and a reversed
selection normalized to offsets 1–3 with caret at 1. It checks empty and
invalid ranges, an invalid selection, unsupported word granularity and caret
movement, and verifies no mutation after the latter. A real password field and
a custom text-role node in the test-only scene do not advertise Text or
EditableText; the password does not expose `nexaui-value`. The same client
then applies the draft using the existing checkbox and button actions and the
host closes normally. The previously failing preimplementation test is
`out/aetheri-atspi-text-before-v1.log`. Initial negative-range assertions
expected a null pointer, but this libatspi version returned a non-null pointer
alongside the expected D-Bus error; the final test requires the error and frees
the returned value. Both original failed runs are retained under
`out/aetheri-atspi-text-invalid-v1.log` and `...-v2.log`.

On the final read-path candidate, the full locally registered Wayland-only
suite passed 124/124 (GCC shared) and 125/125 (Clang static with ASan, UBSan and
leak checking) in private Weston/Xvfb. JUnit contained each registered name
once with no failures or skips; both children and compositors exited 0. The
sanitizer log contains no ASan, LeakSanitizer or UBSan error marker. Logs:
`out/aetheri-atspi-text-full-shared-final.log`, SHA-256
`98e6b07f838b8c0b8733d0230e5f586b5d51f066089f8b913686166e0536d9ee`, and
`out/aetheri-atspi-text-full-asan-final.log`, SHA-256
`201ee25cb1aa7e8e992e8e149e4e3daaf0b7e4bce4b7c26a7d9716fd17325629`.
The test for the 1 MiB reply limit and independent stale-object transitions,
real IMEs, real Aetheri compositor and human screenreader use remain open.

## Text and selection notifications (2026-11-04)

On a stable text-node incarnation, the provider compares consecutive owned
snapshots at Unicode scalar boundaries, preserves a common prefix/suffix, and
sends deleted old text and inserted new text with scalar offsets. Each message
is limited to 64 KiB of UTF-8 payload and split only at character boundaries;
deletion chunks are ordered from the end and insertions from the beginning.
Caret and normalized text-selection changes emit separate events. The existing
8 MiB outgoing budget may disconnect a stalled/large event stream; it is not an
unbounded queue or a guarantee of delivery across reconnection. New or retired
identities do not receive fabricated text deltas.

The independent Settings client registers a libatspi listener and invokes a
test-only model change twice: `Aé🌍` to `Aö🌍`, then to `AöZ`. It verifies the
old and new payloads, scalar offsets and lengths, one caret event, one
selection event, absence of extra caret/selection events on the second change,
and current text. The preimplementation client failed at the expected missing
events (`out/aetheri-atspi-events-before-v1.log`); the changed provider passed
(`out/aetheri-atspi-events-v2.log`). A focused ASan/UBSan run with leak checking
also passed (`out/aetheri-atspi-events-asan-v1.log`). These examples do not test
native delivery at the 64 KiB chunk boundary (covered by the later test below), large-stream backpressure, concurrent client state,
event delivery across reconnect, actual IME, or a human screenreader.

With these changes on base `07e3bdfa3134e30d35ef77c3289591ef1712e43f`, the
complete local Wayland-only private-Weston suites passed 124/124 shared and
125/125 static with ASan, UBSan and leak checking. JUnit has no missing,
duplicate, failed or skipped names; the child and compositor exited 0 in each
fixture. No sanitizer error marker appeared. The shared and sanitizer raw logs
are `out/aetheri-atspi-events-full-shared.log` (SHA-256
`f5e6d1b3b6156df9aa0a7b1e0e85634f34d92576a1a47deb74c519af96127b3a`) and
`out/aetheri-atspi-events-full-asan.log` (SHA-256
`01773f914c38993033f2af92aa6e591a239237307d62148c679272d9c133420e`).
These runs do not close the untested large-event, reconnect or physical and
human acceptance cases above.

## Unicode chunk-boundary replay (2026-11-04)

The production diff/chunk iterator is isolated in the private
`platform/linux/atspi_text_diff.h` and shared with
`tests/test_atspi_text_diff.c`. The test replays emitted insertions and
deletions into a separate byte buffer, checks each scalar position, payload,
UTF-8 boundary and 64 KiB limit, and compares the reconstructed result. Four
layouts place a four-byte scalar around either side of the chunk boundary;
both long insertion and reverse-order deletion are exercised, as are unchanged
text and callback failure. Disabling either UTF-8 boundary adjustment in
separate negative controls made the test fail; the original code was restored.
The independent native client additionally exercises ordinary Unicode deltas,
but did not itself exercise 64 KiB messages over D-Bus or stalled-bus recovery.
The private-Weston AT-SPI subset passed 3/3 with GCC shared and 3/3 with Clang
static ASan/UBSan and leak checking, with no skips and both fixture processes
exiting 0. Logs: `out/aetheri-atspi-diff-native-shared.log` SHA-256
`d190dda72f1a3811d4c60d82ae4299064b3e664c4ed1d2eee87755b36a0be16b` and
`out/aetheri-atspi-diff-native-asan.log` SHA-256
`14482bf9998c4d8f67c4fd409954f039c1988defd2fd98d6878cdea1583b4267`.
The final strengthened replay test additionally checks a callback failure
after one accepted chunk and was rerun separately in both profiles. A fresh
Wayland-only build with AT-SPI and Vulkan explicitly disabled also built and
passed that test. The earlier full-suite counts refer to the preceding source;
only the focused tests named here were rerun for this extraction.

## Native 64 KiB text-event boundary (2026-11-04)

`nexaui_atspi_large_text_wayland_private` now runs a dedicated real NexaUI
text-field host and an independent libatspi client on the private Weston and
AT-SPI bus fixture. Starting from `AZ`, the host inserts 131,072 UTF-8 bytes
between those two characters: 65,534 ASCII characters, a four-byte scalar,
and another 65,534 ASCII characters. The scalar crosses the nominal 64 KiB
split from both insertion and deletion directions. The client checks each of
three insert and three delete messages for exact byte length, scalar position,
scalar length, valid UTF-8 and payload, replays them into an independent model,
then compares that model with a full native Text read. The host detaches and
closes cleanly. This fixture exercises actual Wayland/AT-SPI transport but not
window rendering, a physical display or an Aetheri compositor.

On base `42d15e2626213de6e280b391bb4c0ded93a249a5`, the relevant AT-SPI group
passed 4/4 in both GCC shared and Clang static ASan/UBSan builds with leak
checking; no skips, fixture and child exits 0. Logs:
`out/aetheri-atspi-large-final-shared.log` SHA-256
`a77cf9f69c19b3726c5a0f23427b6a4681f4b92ed3a02e58b94e34f36318a5a4`, and
`out/aetheri-atspi-large-final-asan.log` SHA-256
`04846016e91731206e035d4dc7265096a91d1bb6f6983a1ed763bf57775cf2e2`.
Separate insertion/deletion boundary mutants failed in the native workflow
(`out/aetheri-atspi-large-negative-insert.log` and
`out/aetheri-atspi-large-negative-delete.log`); the original source was
restored and the shared group passed again. These results do not test the
1 MiB text-reply limit, the 8 MiB outgoing budget, stalled readers or
reconnection, all of which remain open.

## Deferred text-value ancestry (2026-11-04)

The portable text-value request previously captured only its immediate parent.
An independent regression demonstrated that changing an ancestor's role or
reparenting an ancestor while retaining the field and immediate-parent IDs
could apply a stale request to the field. The request now captures and
revalidates the complete ordered ancestor ID/role chain, reusing the same
bounded mechanism as multi-selection. The queue remains bounded at 64 requests
and each captures at most `NX_SEMANTIC_MAX_DEPTH` ancestor records. A rejected
request does not prevent a later valid request.

The test covers unchanged ancestry, five distinct role/parent/identity
changes, a fresh request after each rejection, and the maximum supported
semantic depth. With only the new text ancestry check disabled, the unchanged
case passed and all five changed cases failed by applying the stale value
(`out/aetheri-text-ancestor-negative-run-v2.log`, SHA-256
`8c00ff5add5b4cca99308c8db3ba50535444e75f3fa68c68c80a6e17df52a18d`).
The separate AppKit worktree's staged guard was inspected as prior work but
not copied or altered; it overlaps this portable change and must be reconciled.
The comparison follows the SDK's ID/role semantic identity contract and cannot
distinguish an application recreating a logically different ancestor with the
same ID and role in consecutive frames.

On base `238d88a9f42f892e43f8b5fc4a3db877054f695f`, the complete local private
Weston Wayland-only suites passed 127/127 (GCC shared) and 128/128 (Clang static
ASan/UBSan with leak checking), with no skips, missing or duplicated test names,
and fixture/child exits 0. No sanitizer error marker appeared. Native Windows
MSVC 19.38 / Windows SDK 10.0.26100 ran the text-value, ancestry and
multi-selection tests in Debug and Release, 3/3 each. Logs:
`out/aetheri-text-ancestor-full-shared.log` SHA-256
`0a71799c9d1562e5d8c6e18da84611199f9af9a9867995b8de5f228ffffdf83c`;
`out/aetheri-text-ancestor-full-asan.log` SHA-256
`f7e20384599599da488aa3aee7e4068fb7dc882ecbb0a833924cf23e53f3f1cc`;
`out/aetheri-text-ancestor-msvc-tests.log` SHA-256
`967069d77334d09352c0989b7dfdf24e2dda3c1584b5e5449a489ffcb9db6ecb`;
and `out/aetheri-text-ancestor-msvc-release-tests.log` SHA-256
`1e044f68c386fd73a249cee6f87222b8e598082ebf48ebf6daee1ce64af4160b`.
These checks do not implement or approve Linux native text writes; their
connection lifetime, model acknowledgement and policy guards remain open.

## Text-value completion prerequisite (2026-11-04)

The private core bridge can now attach an owner-thread completion recorder to
a guarded text-value request. Admission itself produces no success result;
the recorder observes success only after the text buffer accepts the edit,
including an equal-value no-op. Expiry, an invalidated guard, a changed policy
or context destruction records cancellation. The guard retains the recorder
through its single completion, and rejected admission does not call it. The
recorder is explicitly prohibited from entering NexaUI or calling native or
application code; frame publication is a separate event.

The focused regression exercises delayed success before frame publication,
equal values, guard revocation, omitted widgets, disabled fields, rejected
admission and context destruction. On base
`b0c6cb95ba60daa1e86c493fe07f079538ebf1f4`, the three text-value tests passed
in the GCC shared Wayland-only and Clang ASan/UBSan static configurations; the
existing two portable text-value tests also passed with Windows MSVC Debug.
That prerequisite alone did not implement AT-SPI EditableText. The following
section records the later native work and its remaining validation gaps.

## Whole-value AT-SPI editing (2026-11-04)

The Linux provider advertises a partial EditableText interface on actual,
non-protected writable text fields. `SetTextContents` copies at most 65,536
UTF-8 bytes into the bounded native queue and defers its boolean reply until
the owner thread's model replacement completes or the request is cancelled.
It rechecks the context binding, connection generation, object incarnation,
model identity and revision, edit policy and deadline; the core also checks
ancestry, disabled/modal state and the actual widget. A two-second monotonic
deadline clamps native poll waits, and expiration revokes even a request already
forwarded to the core. One outstanding write per field is accepted. Other
At that step other EditableText operations returned `NotSupported`; Text movement, selection and
geometry remain separate work. Read-only capability changes retire the old
native path to prevent a cached editable interface from outliving the policy.

The independent client verifies Unicode and equal-value replacement, rejects
newline and oversized input, detects unsupported insertion, confirms there is
no reply or model mutation before a frame, and tests duplicate, disabled,
read-only, omitted, queued timeout, forwarded timeout and detach cases. It also
checks that a timed-out write cannot apply in a subsequent frame. The actual
Settings client replaces the name and checks the host's applied model and clean
exit; secret/custom nodes remain excluded. A first detach test exposed a lost
reply when the last window closed its bus; the corrected bridge writes pending
replies before its nonblocking connection teardown.

On base `3b7e7427ac4aa15d1ccb9277cca4091d8106dae5`, the four registered
AT-SPI tests passed in private Weston/Xvfb with GCC shared and Clang static
ASan/UBSan with leak detection; after adding the explicit unsupported-insert
check, the large-text native test passed again in both configurations. This is
a virtual compositor, not a real
Aetheri session or human screenreader acceptance. Connection-loss/reconnect,
backpressure, allocator reentrancy, modal changes and actual IME overlap need
additional native write tests (the connection and rebind cases are addressed
below); the two-second bound assumes the application
continues polling its window. Full EditableText support is not claimed.

## External replacement during IME composition (2026-11-04)

The Settings text-input-v3 fixture exposed stale preedit after a changed
external semantic replacement. The previously queued IME commit was already
rejected by its text/selection revision, but the preedit was still displayed
over the new value. After an actual replacement, the field now disables its
active IME before painting; frame completion reactivates the focused field
with its updated surrounding text. An equal-value request preserves composition.

The controlled v3 fixture checks the unchanged input path, replacement with a
queued UTF-8 commit and nonempty preedit, the resulting value and cleared
preedit, disable/re-enable, and the equal-value case. Before the fix, the new
replacement test failed specifically because the preedit remained nonempty;
the other original test passed. On base
`33e0a711a8254b40189814b6c9ec3fd403415bdd`, all three Settings v3 tests,
three core text-value tests, both Wayland v1/v3 protocol tests and the native
Settings AT-SPI test passed in the GCC shared and Clang ASan/UBSan static
profiles; the two existing text-value tests also passed on MSVC Debug. The
v3 fixture calls the same semantic admission path directly, while the separate
private-Weston test exercises D-Bus and model application. Their combination
is not an end-to-end AT-SPI plus real IME test; that, physical input and human
screenreader acceptance remain open.

## Pending writes across connection and context changes (2026-11-04)

The independent libatspi/D-Bus fixture now tests both a queued request and a
request already forwarded to the core in each of two cases: disconnection of
the provider's private bus connection and binding the window to a new context.
For disconnection the pending caller observes a D-Bus error, the old context
submits a frame offline, and the client discovers the reconnected application
on a new bus name. For rebinding the old context is deliberately kept alive
and submits a frame after the new context has been published; the caller gets a
negative reply and the replacement field has a new native object path. In all
four cases the abandoned payload does not alter the model, and a fresh request
actually changes the new field and can change it back. The test then exercises
the existing detach case and clean shutdown.

On base `3bcbb05cee748e283a63a4e69018fdb23f0d8e5b`, the independent native
test passed under private Weston/Xvfb with GCC shared and Clang static with
ASan/UBSan and leak detection. Two separate controlled mutants bypassed the
forwarded-request guard only after disconnection or only after rebinding. Each
caused the corresponding unchanged-model assertion to fail (CTest exit 8);
the source was restored and the normal shared test passed again. Logs:
`out/aetheri-atspi-reconnect-negative.log` SHA-256
`cb906c5f854bd4618d837b02dfa88982afcda6ea75306aaeb3cd33028708d0ae` and
`out/aetheri-atspi-rebind-negative.log` SHA-256
`fb32d79dd7b084514e0b5491c335eb7a9740830388cbcf1439d1bbfcd4b34cb2`.
These controlled connection closures do not cover a killed bus daemon,
network-like backpressure, or simultaneous application allocator reentrancy.

## AT-SPI insert and delete (2026-11-04)

Writable built-in fields now accept `InsertText` and `DeleteText` through the
same bounded, deferred completion and lifetime guard as whole replacement.
Positions are Unicode scalar offsets in the published snapshot; insertion's
length is a byte count and a prefix ending inside a UTF-8 sequence is rejected.
The private core range operation validates byte boundaries and changes the
real text buffer, retaining its pre-edit selection in the undo record. A
changed range also resets an active IME composition. Each range is tied to the
source frame: a new published frame before routing cancels it instead of
applying old offsets to different text. This conservatively cancels on even an
unchanged new frame. At this stage clipboard operations and text
caret/selection setters remained unsupported.

The independent client tests mixed-width insertion, byte-limited prefixes,
deletion including a combining mark, resulting caret positions, two undo
steps, invalid offsets, split UTF-8 prefixes, disabled targets and a queued
range invalidated by a local text event. A separate independent client inserts
and deletes in the real Settings field and verifies the applied value. It also reruns the earlier deadline,
disconnect, context-rebind and teardown cases. On base
`9feb6aee7d6b0ab753ff05c185ec30069f90902f`, the private Weston native test
passed in GCC shared and Clang static ASan/UBSan configurations. Deliberately
removing only the source-frame check caused the local-edit cancellation
assertion to fail (CTest exit 8; `out/aetheri-atspi-range-negative.log`,
SHA-256 `8745a2b9d2264d41093e48712d3fcafdb10ea3aa8c2f5bb8688a89033e8c7cdd`);
the production source was restored and rebuilt. The core text-value and
Settings IME suites also passed in both Linux profiles, and the existing
Windows text-value tests passed in MSVC Debug. A deterministic allocation
failure test also checks that range edits preserve text, selection, history
and allocations on OOM in both Linux builds. Native tests use a virtual
compositor and do not prove real IME, screenreader or Aetheri acceptance.

## AT-SPI caret and text selection (2026-11-04)

The partial Text interface now implements SetCaretOffset, AddSelection,
SetSelection and RemoveSelection. The existing bounded owner-thread queue and
completion guards apply to selection requests, including snapshot-frame,
context, window, disabled and protected checks. Read-only built-in text can
still be selected. The existing text buffer accepts only extended-grapheme
boundaries; a legal scalar offset inside a grapheme therefore returns false
rather than silently moving it. One active selection is supported. A changed
selection also resets an active IME composition before the focused field is
reactivated; an unchanged selection does not.

The independent client moves the caret, adds/changes/removes the selection in
the real Settings field, checks the reported offsets and the exact ordered
caret/selection notifications, and rejects a second selection and invalid
index. The separate native client checks read-only selection, a rejected
inside-grapheme caret position, deferred completion and cancellation after the
field becomes disabled. On base `1bfdf4ebf12d044eedf7d51a7d70acce0ed276f9`,
the private-Weston AT-SPI group passed 4/4 in both GCC shared and Clang static
ASan/UBSan configurations; the strengthened focused native and Settings tests
passed again, as did the core text-value and Settings v3 IME tests. Existing
MSVC Debug text-value tests also passed. Native clipboard callbacks, complete
scalar-position support within graphemes, real IME/screenreader and compositor
acceptance remain open.

## AT-SPI clipboard operations (2026-11-04)

CopyText, CutText and PasteText now use the same bounded deferred request and
the callbacks supplied on the actual submitted text field. The provider does
not retain a callback or an application context in its native snapshot.
CopyText has no boolean wire result: failure returns a D-Bus error. In this
libatspi version its C wrapper can return true while also setting `GError`, so
clients must inspect the error. CutText and PasteText return a boolean. Cut
copies first and checks the guard and field identity/revision again before it
edits; failure to allocate the edit after a successful copy leaves the text
unchanged, although the clipboard may already have changed. Paste rechecks
after both clipboard callbacks and after semantic allocation, limits input to
65,536 bytes, rejects invalid UTF-8 and NUL, and normalizes line breaks to
spaces in single-line fields. Text edits use the existing undo and IME paths.

The independent libatspi client covers Unicode copy/cut/paste, undo, clipboard
errors, replacement of the model during a callback, and detachment of the
native bridge during cut. Internal tests also revoke the guard and replace the
model during both paste-read phases, and reject oversized or invalid input. A
targeted mutant bypassing post-copy revalidation caused the cancellation test
to fail (`out/aetheri-clipboard-negative.log`, SHA-256
`63ef4e27292ce05ca95b20eed2fa687c1f6becaa9f2888be730f375799593769`;
source restored and rebuilt).
The Settings client verifies both refusal without a Wayland input serial and
copy/cut/paste after virtual keyboard input on private Weston; its read uses
the same window's owned source, not a second application. The focused native,
Settings and core tests passed with GCC shared and Clang ASan/UBSan static on
base `f59b64bd8e6b6db13cf4d6b06bc4f0e648796320`. MSVC Debug's existing
text-value tests passed. An installed input method, an external clipboard
owner/consumer, stalled clipboard callbacks and actual screenreader/Aetheri
acceptance remain open. The two-second request deadline is checked before and
after callbacks; synchronous callbacks can themselves block longer.

## Independent Wayland clipboard peer (2026-11-04)

An additional private-Weston Settings test uses `wl-paste` and a foreground
`wl-copy` as separate Wayland processes. After virtual keyboard input supplies
a valid serial, Settings copies `ö` through AT-SPI and the independent reader
verifies its exact UTF-8 bytes. The external owner then publishes `Peer 🌍`;
Settings pastes it through AT-SPI, verifies the resulting field value, applies
the expected final model and exits cleanly. The fixture waits for the external
clipboard state rather than assuming that the owner's D-Bus reply also means
the Wayland selection has already reached another client. It restores virtual
focus after the helper client has obtained clipboard access.

On base `90a3b7f69b2f412ae5ddfddbcb6ddc2f9f1fdb0f`, the peer and owned-source
Settings tests passed 2/2 in each of the GCC shared and Clang ASan/UBSan
profiles. The peer was Alpine `wl-clipboard` 2.2.1-r1 extracted into `/tmp`;
the test is registered when both tools and `xdotool` are found at configure
time, and otherwise remains unverified. It is a private compositor with
virtual input, not a physical desktop, real screenreader or Aetheri-compositor
acceptance. External owner exit, very large/slow transfers and input-method
overlap remain to be checked separately.

## Single-transfer Wayland paste (2026-11-04)

Native-window AT-SPI paste now fetches once into its existing 65,536-byte
limit. Keyboard paste uses a dynamically sized, bounded Wayland transfer, so
it does not consume a one-shot owner in a separate size query. Existing custom
clipboard callbacks retain their query/read contract; AT-SPI rejects a length
change between those calls. The Wayland transfer retains its 32 MiB and
five-second bounds. A private fixture supplies text only on its first receive:
both the real Settings field's AT-SPI request and Ctrl+V previously issued two
receives and failed the expected-value check; both now issue exactly one and
insert the expected text. The independent `wl-copy --paste-once` Settings
test additionally verifies a real external owner and its clean exit, although
that process alone is not a reliable count of rapid receive requests.

On base `708c7a127a96bf160a5b8381532c4de85ea28239`, the GCC shared private
Weston suite passed 135/135, and the Clang ASan/UBSan suite passed 136/136,
including the independent peer and keyboard tests. A separate X11-plus-Wayland
library build succeeded. Wayland streams do
not advertise an expected byte length: an owner closing after a valid UTF-8
prefix cannot be distinguished from a complete short transfer. Large/slow
transfers, IME overlap, installed screenreaders and actual Aetheri-compositor
acceptance remain open.

## Clipboard transfer deadline and stalled owner (2026-11-04)

On base `37813559cee433f92fd7f4324aa24e226527b3ec`, a deterministic Wayland
offer writes a valid prefix and keeps the pipe open. Both the Settings AT-SPI
paste and Ctrl+V reject the incomplete transfer after the existing five-second
deadline, leave the draft intact and accept a subsequent typed character.
The test observes the actual receive and releases its held descriptor only
after the request returns. This checks recovery after the timeout, not UI
responsiveness during it: the current transfer runs synchronously on the owner
thread and can block the view for five seconds. Async transfer remains needed
for I05/W10 and for a two-second AT-SPI deadline that cannot be overrun by a
synchronous callback.

A second regression delays the window allocator during receipt. Before the
fix, the five-second deadline was checked only after a read would block, so a
completed read after the delay was incorrectly applied; the test failed with
an applied paste. The reader now checks monotonic time between iterations and
again after pumping windows, including when bytes or EOF are ready. It cannot
preempt a blocking application allocator or other synchronous callback; it
rejects the transfer when control returns. The delayed-allocator test passes
with GCC shared and Clang ASan/UBSan. A Wayland owner closing a valid UTF-8
prefix remains indistinguishable from a complete short transfer.

The focused private-Weston clipboard group exposed an intermittent pre-existing
failure in `nexaui_settings_wayland_atspi_clipboard_once`: it passed in the
previous full suites and on repeated reruns, but failed in both current builds
with the field still `AöZ` after a successful empty paste. In the failing
Weston 14.0.2 trace, the compositor requests the new source immediately after
selection, before Settings requests its offer; `wl-copy --paste-once` can
therefore terminate before Settings reads it. Weston's clipboard manager may
then publish a cached replacement, racing the existing offer. The client trace
is `%TEMP%/nexaui-clipboard-once-trace2.txt`; the separate compositor trace
was overwritten by a later local run and is not retained. This
test has not been disabled or counted as a pass. The other seven tests in each
focused group passed, including regular separate-process clipboard exchange;
the deterministic one-receive fixture continues to test the one-shot path.
The independent client's diagnostic now prints the actual test value and frees
it before reporting failure, so the failure path does not leak that value.
