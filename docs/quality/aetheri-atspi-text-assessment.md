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
   and implement text/caret/selection notifications. Preserve secret suppression,
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
`NotSupported`. No EditableText interface or text-change/caret/selection
notifications are implemented. Text capability, field identity and ancestor
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
