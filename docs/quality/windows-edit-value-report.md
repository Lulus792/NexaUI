# Windows editor ValuePattern: local candidate validation

Recorded 2026-10-21T21:30:55.204056+00:00, based on5f7680f7d00a950f759d17d98520405901f9419d. This candidate
adds readable AND writable Windows ValuePattern for standard owned text/numeric
editors. It is a local implementation with the following exact validation;
independent review and external full SDK CI are still separate gates at this time.
This report does not retroactively accept the earlier119/120 Windows Shared run.

## Contract and implementation

Unprotected editor values, including truly empty strings, are exposed consistently
through ValuePattern and strict native property reads. Custom semantic SET_VALUE
without an owned model remains read-only. Native UTF-16 input is bounded and
validated, converted to at most65,536 UTF-8 bytes, and copied into a64-entry
native queue. The owner copies it into its own bounded queue; no editor/context
pointer is retained by COM callbacks. Payloadless SET_VALUE remains unsupported.

The actual next widget rechecks model lifetime identity, application text/policy
revision, parent, inherited disabled state and active modal scope. It applies
one atomic whole-text edit. Undo restores the original selection, equal values
preserve redo/selection, and edit allocation failure preserves model state.
Application whole-model replacement/policy changes and omitted fields expire
older requests. Numeric fields change their draft, without submission/commit.
Read-only text fields retain focus, selection/navigation and Copy while rejecting
edits, undo/redo and IME mutation. Public descriptor layouts remain unchanged.

A retained native lifetime lease can revoke an already forwarded request after
window disable/re-enable, context switch/back, detach/reattach or HWND destruction.
Context destruction frees pending payloads/leases. Native getters use owned,
locked snapshots. Value/read-only/enabled/availability/password event arguments
are owned before call-outs and survive reentrant snapshot teardown. Protected
values are suppressed in queries AND event preparation, including custom
semantics that deliberately contain plaintext in the test fixture.

## Executed local validation

Windows11/x64, MSVC19.38.33145/toolset14.38.33130, SDK10.0.26100.0, C17:

| Configuration | Core selection | New external native cases | Existing native regression |
| --- | ---: | ---: | ---: |
| Shared Debug/ASan |6/6|3/3|4/4|
| Static Release/O2/NDEBUG, no ASan |6/6|3/3|4/4|

Core selection: text_value, text_field, numeric_field, semantics, text_buffer,
text_buffer_alloc. Deterministic queue and semantic-reserve OOM,64-entry/64KiB
bounds, copied payloads, Unicode/empty/multiline, reverse selection/undo/redo,
policy/model/parent/instance changes, modal and ancestor disablement, expiration
and destruction are exercised. Text-buffer allocation tests retain their
sensitive scrub/history checks and test whole-value OOM as well.

The final new external C17 host/client programs cover STA, MTA and library-owned
COM. They verify strict properties/patterns, preset/empty/Unicode values, owner
admission versus reflection, duplicate admission, malformed UTF-16, byte-expansion
and maximum bounds, owner-copy OOM/retry, untouched peer fields, real value and
state callbacks, protected custom transitions, stale elements, context roundtrip
AND context destruction, disable roundtrip, detach and window teardown. They link
only system COM/UIA libraries in the client. Private desktop/job isolation is
mandatory for every local native run; input desktop remains unchanged and own
processes/desktops close. These are no physical input/Narrator/IME/DPI results.

The older native suite retains the strict blocked-owner CurrentName regression,
all selection/backpressure/lifetime cases and Design A. The protected-value
assertion now requires denial or NotSupported instead of an empty BSTR default.
Both clients now allocate actual BSTR inputs according to the client interface;
a client allocation failure exits the test instead of masquerading as a rejected
setter. No old deadline, queue bound or renderer behavior is relaxed.
Installed public-header C17 consumers build and run for static and shared SDKs,
referencing all three new public APIs. Real compiler/link PDB paths and source/
program hashes are recorded separately under out/edit-value-final-v1/.

## Counterexamples and corrected test assumptions

Identical final new host/client EXE/PDB pairs are also run with two deliberate
private SDK mutations: removing editor Value support fails the first supported
field read; bypassing the owner-time native lease check applies an old request
across context roundtrip and fails the unchanged model assertion. Original
expected failures and private SDK sources/programs remain preserved.

Earlier new-fixture failures remain preserved under out/edit-value-owner-v1/
and out/edit-value-native-v1/: a clipboard member typo and guard include ordering
blocked compilation; a count oracle confused context-owned paint cleanup with
payload cleanup. Later external observations corrected three unsupported test
assumptions, without production workarounds or increased deadlines:

* Strict unsupported pattern availability can be NotSupported rather than BOOL;
  the ordinary property is false, with no pattern. Live empty fields separately
  require a supported empty BSTR.
* SetValue waits during the2000ms owner hold (WAIT_TIMEOUT258), while the model
  remains unchanged. After normal pumping it is admitted, and remains unchanged
  until owner routing plus the next widget. Owner-independent completion of UIA
  actions was an extra, unpromised test assumption, not a fixed latency defect.
  The original strict CurrentName blocked-owner test remains unchanged.
* Route counts include other native events/focus/actions. A public duplicate-
  admission probe now proves the specific text payload reached the core queue.
  Retired ValuePattern reads can normalize to S_OK/empty; a separate element-
  lifetime error and refused write prove staleness and protect the new model.

A subsequent compiler/signature review found that the client SetValue method
takes BSTR, unlike the provider LPCWSTR method. Earlier fixture calls used wide
string pointers. Those programs/logs are retained, but are superseded by14 final
native passes with actual BSTR allocation, plus both controlled failures using
identical final host/client EXE/PDB pairs. Constant false host CHECK branches
were written as explicit fatal returns, preserving failure behavior without
compiler-warning suppression.

Own source review also identified a real owner-revalidation gap: a parent can
be disabled through its interaction region even when its semantic descriptor
still says enabled. A new public-API counterexample actually applied the queued
write and failed before the correction. Programs/PDBs/DLL and source are retained
under ancestor-region-counterexample/. The ancestor check now includes disabled
and inactive interaction regions. The final six-core and14 native cases are
rerun against this corrected library. An initial placement error in the new test
blocked compilation; that source/log is preserved separately from the actual
runtime counterexample.

An initially missing Release PDB in the earlier primitive-only fixture was
corrected there by explicit private /DEBUG linking and a targeted retest; final
builds produce their real PDB paths from the start. Historical red data and all
prior frozen evidence are retained; green controls do not erase them.

## Independent review corrections (v2)

The first independent source/evidence review rejected v1 with two P2 defects.
A context allocator called under the exclusive bridge SRW lock can synchronously
reenter WM_ENABLE and deadlock. A separate allocator callback during semantic
reserve can revoke a valid request after its last lease check, yet v1 applies it.
Both original defects were reproduced against the frozen v1 DLL before acceptance
of the fix. The original review, programs and traces remain unchanged.

Routing now moves payload ownership into the owner stack and pins the bridge
across the unlocked core call. A monotonic request key and in-flight flag let it
re-resolve the surviving queue stub after reentrancy; detach cannot free that
payload. Queue-full, OOM and active-frame retry retain valid pending work.
The core rechecks the native lease after both semantic-reserve allocator call-outs,
before the atomic text-buffer edit.

Six native cases cover disable/re-enable and native detach from route allocation,
reserve allocation and reserve deallocation. Each fresh context warms both region
buffers before arming the reserve callback. All six assert unchanged text,
selection, Undo and Redo, and run within each of the six native COM/configuration
combinations. Both core configurations pass six cases; all14 native suite runs and
two installed consumers pass. Evidence is in out/edit-value-callout-v2/local.json.
Three controls use the identical final host/client EXE/PDB pairs with the old DLL:
route-disable enters the callback and times out at the new15s private diagnostic
budget; reserve allocation and deallocation return normally but apply4096 bytes
instead of retaining the original9 bytes. All three jobs/desktops are cleaned,
and the user's input desktop is unchanged. Existing test deadlines are unchanged.

The first reserve fixture accidentally triggered the other region buffer's10240
byte allocation before the guard. It passed without reproducing P2#2 and is
explicitly superseded by the warmed fixture's8192-byte allocation/256-byte release.
Its logs and unsuccessful expectation are retained in baseline-probe/ and
baseline-results.json. This was a test-target correction, not a product fix.

Microsoft documents synchronous WM_ENABLE before EnableWindow returns and the
nonrecursive exclusive SRW contract: [EnableWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enablewindow),
[SRW locks](https://learn.microsoft.com/en-us/windows/win32/sync/slim-reader-writer--srw--locks).
The runtime counterexamples establish the specific NexaUI paths above.

## Reentrant admission correction (v3)

The independent v2 review confirmed both P2 fixes, but rejected v2 for a P1
core-queue overflow: with 63 pending targets, a user allocator can admit a nested
request into the last slot before the outer request appends without rechecking.
The preserved original ASan run reports heap-buffer-overflow. The v3 admission
captures the scalar target/frame before allocation, then rechecks completed-frame
state, frame identity, capacity and duplicate IDs after the callback. Rejected
copies are freed and LIMIT retains the native route's existing retry behavior.
No borrowed semantic-node pointer is used after allocation.

The permanent regression covers both last-slot consumption and nested admission
of the same target, checks which values actually apply, verifies retry after the
next frame and checks all context allocations are released. The identical final
ASan test EXE/PDB with the frozen v2 DLL reproduces the overflow. The initial
reproducer, final counterexample and new configuration results are retained under
out/edit-value-reentrant-v3/. Prior v1/v2 frozen source and evidence hashes were
rechecked without modification. Full v3 evidence and review are recorded in the
separate out/edit-value-review-v3/ package; v2 remains a rejected candidate.

## Remaining gates and scope

Independent source/evidence review, full external static/shared/sanitizer CI and
separate NexaCloud application adoption remain pending for this new candidate.
Local Windows ASan is not LeakSanitizer or whole-platform acceptance. Native
Text/TextRange, RangeValue, AppKit setters, AT-SPI EditableText, real IME/Narrator/
Mixed-DPI and visible resize/share acceptance are separate work. User manual
testing stays paused; the powered-off user Mac is not contacted. No main merge
or NexaCloud dependency update is part of this change.

Microsoft primary references checked2026-10-21: [Value guidelines](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-implementingvalue),
[SetValue API](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/nf-uiautomationcore-ivalueprovider-setvalue)
and [Edit control requirements](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-supporteditcontroltype).
The2025 guidelines include editable multiline Value support while the older
2024 SetValue remarks exclude it. This candidate supplies whole-string Value
access for both modes; it does not claim the separate ITextProvider contract.
