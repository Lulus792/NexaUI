# Linux AT-SPI text adoption assessment

Baseline `05e28bb16c1e60ac339cd9428ff2d3afbafc21f6`; current working
candidate has the separate Wayland-only and Settings changes documented in
[aetheri-adoption-assessment.md](aetheri-adoption-assessment.md). This document
is a source assessment, not a completed implementation or native acceptance.

The independent Settings client now confirms a limited AT-SPI Text read path
for its editable name; `EditableText` is still absent. The provider's snapshot in `platform/linux/accessibility.c`
contains copied semantic name, description, value, bounds, role and state. The
current candidate also copies an internal completed-frame caret/selection snapshot
for genuine non-protected text fields; positions are UTF-8 byte offsets, not
AT-SPI character offsets. The public `nx_text_field_selection` itself is an
owner-thread model query and must not be called from a D-Bus query on a retained
object. The core already offers `nx_semantics_request_text_value` for whole
replacement with UTF-8 validation, owner-thread admission and deferred widget
application. It is not yet an AT-SPI adapter and admission is not successful
application. Reuse it rather than introducing a second editor.

A separate in-progress AppKit worktree has staged changes to the same portable
`src/nexaui.c` and `tests/test_text_value.c` that capture and check the full
ordered ancestor ID/role chain for deferred text requests. They are not part of
this candidate, and their presence is not a review or acceptance. Coordinate
and independently validate the exact final core fix before relying on deferred
text writes after ancestry changes; do not overwrite or silently copy that
worktree's staged changes. Linux-native queued requests also need their own
binding/incarnation, disabled/protected/modal and connection-lifetime guards.

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
editing, notifications, full protocol coverage and the ancestor guard remain open.

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
`NotSupported`. No EditableText interface is implemented. Text capability, field identity and ancestor
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
