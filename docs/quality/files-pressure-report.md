# Files IME pressure and retired-view lifetime — scoped current SDK

Implementation: `5f14a9ca74ce3f389c5475cc41016589548307c4`.
Installed SDK remains0.1.33/source`1e00a6f4f3a0ab3a7da1aa35a36f02679421366b`.
Original Files stays frozen at`7483286bcaafbeaced5831f72cb4d0e0cad9a158`;
the separate producer stays at`d56d69523c33a48648af343e7d36526f84518c90`,
with its two recorded generated files. No SDK production source, public header,
renderer, version or Aetheri pin changes.

This extends the generated Files IME consumer. It is **not full current-SDK
document-child or normal application acceptance**. Preserve the historical
full0.1.26 [Files result](files-paste-report.md) with its own original
launch/Terminal/editor evidence; current scoped runs do not relabel it.

Affected evidence: B01/B03/B04, T01/T03/T04, I01/I02/I05, A02/A04,
P04/P05 and Q02/Q03/Q06/Q08. All status columns remain unchanged. This uses
existing `nx_text_field_host_ime_revision`, `nx_context_queue_event`, public
text-buffer/selection/history and native clipboard/accessibility contracts;
there is no new SDK API or required core dependency. The separate consumer can
exercise this exact bounded scope now; normal Aetheri pin/owner adoption still
requires its own remaining gates and is not changed here.

## Changes and coverage

Explicit `FILES_IME_SDK_VERSION=0.1.33` retains the exact0.1.26 IME default and
exact0.1.23 unchanged-host default. The supplied revision must still match the
installed SDK declaration. The current Clang profile separately generates one
equivalent float predicate in original `layout.c`; two generated view predicates
also make their zero/nonzero comparison explicit. Original exports and strict
conversion diagnostics remain intact.

The public-host-token IME component test grows10→14 cases. Its native-window
guard remains an explicit component test guard, not compositor evidence. Added
cases cover invalid-then-valid packets before one frame, event-array overflow
followed by a valid packet, and real view destruction/recreation with either an
incomplete transaction or an accepted allocated preview before paint/model
publication. Rejected drafts cannot confirm until explicit clearing. Old tokens
cannot edit a new draft; current edits and Undo still work.

Private producer commands28/29 and `--transactions --pressure` check native
retention, invalid semantics, disabled confirmation and unchanged fixture files.
Existing Unicode replacement/delete/empty/selection/Undo, cancellation, foreign
clipboard/query policy, fallback, five font-startup refusals and original
create/rename assertions remain. These are IME text transactions; no new
filesystem-delete operation or second file-operation owner is introduced.

The independent AT-SPI clipboard peer now treats `GError` as failure before an
expected semantic refusal. Its optional timeout guard reuses the Editor's
foreign-stream helper and real foreground `wl-copy`. Only that foreign process
loads the gate. Three incomplete UTF-8 bytes are consumed while the writer
remains open beyond the unchanged one-second libatspi deadline. The peer must
exit1 with actual NoReply; truncated EOF then preserves original name/files.
One deliberate negative per profile is separate from ordinary replies.

## Actual execution and evidence

Host: Alpine-Dev/WSL, UID65534, Clang22.1.3/C17 and
`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`.
The already source-bound SDK binaries are reused. Profiles: Release-static,
Release-shared and Debug-O1 ASan/UBSan with LeakSanitizer enabled.

| Check | Actual result |
| --- | --- |
| Session/view and generated paste/appearance/IME | 9/9 CTests per profile,27 total;14 IME cases each |
| Final native IME/clipboard/create/rename | Three passes,350 checkpoints and66 captures each |
| Deliberate native NoReply | Three required peer failures; actual writer closure and unchanged name/files |
| Exact preceding Files peer | Falsely reports successful refusal on actual NoReply; final guard rejects it |
| Compiled missing rejection barrier | New case10 detects the changed original draft |
| Compiled missing preview destruction | All14 assertions run; LSan detects retained preview allocations |
| Complete cross-profile images | 198/198 bytewise pairs equal;198 captures retained |
| Visual review | Nine full outputs inspected: Unicode replacement and both pressure rejections |
| Default/version/source/output controls | Five specific refusals; original defaults remain exact |

[Local receipt](files-pressure-local.json), [source binding](files-pressure-source-binding.json)
and [image receipt](files-pressure-images.json) retain commands, hashes and limits.
Binding checks54 existing owner/test objects and14 regenerated host/layout/producer
files, with148 export-to-Git input records: both consumer snapshots and104 frozen
original Files/producer rows. The earlier83 controller is setup/history;
the final86 controller supplies all three final native tasks.

All seven required [exact-source CI jobs](https://github.com/Lulus792/NexaUI/actions/runs/42733255963)
pass on attempt1; two optional jobs are skipped and no CTest is skipped.
The [CI receipt](files-pressure-ci.json) checks all seven decoded logs, test
summaries and required installed/add_subdirectory consumer steps. The root
matrix does not build this separate Files consumer; its scoped local evidence
remains distinct from SDK/package CI.

[Setup and preserved failures](files-pressure-setup.json),
[five specific refusal controls](files-pressure-guards.json) and the
[verified raw archive](files-pressure-archive.json) retain the private runtime,
sources, objects, native captures, negatives and complete logs. Unchanged SDK,
Wayland/wlroots and foreign-writer prerequisites are referenced by verified
immutable archive hashes. Borrowed font files remain outside the archive.

## Local libc boundary and preserved failures

Original Files launch needs `posix_spawn_file_actions_addclosefrom_np`. Local
musl has no such symbol; the earlier glibc SSH endpoint is unavailable. No VM
was resumed, and no FD-closing fallback, fake child or global installation was
added. Scoped builds force an **aborting test tripwire only into original
`launch.c`**. No successful launch is supplied; final runs never reach it.
Real original Terminal/editor executables were built separately and supplied,
but this runner does not invoke them.

Thus the9 groups exclude the18 additional original child/regression groups in
the historical27-group full profile. Current-SDK document-child launch,
FD/group/environment assertions and child runtime remain open. The optional
musl bridge reuses Example23's Terminal syscalls only; it does not implement
Files' missing closefrom action or normalize termios.

Retained setup failures include strict float diagnostics, relocated layout
header inclusion, Terminal libc declarations, the missing Files spawn action,
a malformed private tripwire header, a not-yet-linked real editor, and a private
runtime collision choosing unpatched wlroots. Corrected runtime uses the same
checked patched wlroots as the producer link and checks its two required symbols.
The failed runtime is preserved. Binding-script attribution/schema failures and
WSL Git-path failure are kept separately, not counted as product passes.

## Remaining work

Re-run the full current-SDK original child/document regression on the required
libc without the tripwire. Continue wider interruption/provider/clipboard
pressure/deadline/error combinations, installed IME/font/screenreader and actual
Aetheri compositor/session acceptance. Physical/human, design, performance and
normal-owner adoption remain separate gates. Bounded atomic text edits supersede
stale replacement/delete notes only for their tested scope. Packet6, packets7–8
and all103 requirement statuses remain active. Aetheri source, pin, build,
defaults and processes stay independent.
