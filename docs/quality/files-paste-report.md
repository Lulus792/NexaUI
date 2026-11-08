# Files whole-proposal paste admission — 8 November 2026

Consumer implementation: `713c26ee3db9c9fd3da61370bd290d54732b29ef`.
The published checkpoint `03ff23e1716fc0bc70135e29e149212026fe2e55` has
[seven successful required CI jobs and two optional skips](files-paste-ci.json).
These SDK jobs do not execute the separate external Files consumer matrix below.
Affected IDs: B03/B04, T01/T03, I05, A02/A04, P04/P05 and Q02/Q03/Q06/Q08.
This continues packet6 without closing those full catalogue requirements.

The optional generated Files IME variant now requires installed SDK0.1.26,
source `794be7cec49f427fcc56e2604cddb44245347021`. It reuses
`nx_text_field_set_paste_validator`; there is no new core API/ABI/version change.
The default-OFF original0.1.23 consumer remains separate. Original Aetheri Files
`7483286bcaafbeaced5831f72cb4d0e0cad9a158` still supplies87 checked app/child
inputs; private producer `d56d69523c33a48648af343e7d36526f84518c90` supplies17
checked inputs. No active Aetheri checkout, SDL app, model or reviewed pin changes.

## Behavior and ownership

Each field has persistent application-owned validator data. Function/user identity
stays stable across frames, preserving current asynchronous transfers. The SDK
supplies current bytes, replacement offsets and normalized insertion. Files checks
the complete255-byte result before copying prefix/insertion/suffix into a256-byte
stack buffer, then calls its original name/query validator. No second editor,
clipboard transport or new app naming rule is introduced.

A rejected paste leaves field text, reversed selection, undo and redo untouched.
The existing deferred rejection marker supplies the original localized error and
confirmation barrier. A second coqueued paste cannot evade that pending barrier.
Assistive Paste reports rejected rather than claiming an edit was applied.
Stable controller IDs/scopes, normal file Cut/Paste, create/rename/unknown-outcome
and child/session ownership retain their original code.

The original barrier still makes the field read-only until explicit clear. Undo
is not a new way around it. Explicit clear intentionally resets history through
the existing set-text operation. Other post-edit defensive rollbacks also retain
their old behavior; this step specifically admits clipboard proposals before edit.
Single-line CR/LF normalization,32-MiB/five-second keyboard transport bounds and
the255-byte accepted app draft remain distinct. A65,537-byte foreign proposal is
tested; full-budget/pressure/deadline acceptance is not inferred.

## Executed builds and independent clients

The same SDK source was separately rebuilt for the existing private unprivileged
x86-64 glibc Linux VM, with GCC16.2.1, Wayland1.26.0 and actual AT-SPI compilation.
Shared Release, static Release and static Debug ASan/UBSan/LSan each pass23 affected
SDK CTests and an installed C17 consumer. Three separately compiled metadata
probes report exact revision794be7c,version0.1.26 and compiled-feature mask116:
Wayland, fractional scale, poll FD and AT-SPI; no X11. These are compiled features,
not a general runtime/service-admission claim.

Each Files profile passes **27/27 CTests** and compiles50 C17 translation units
with `-Werror`. Existing deadlines remain. Included are the original app/child
tests, ten transaction-controller cases,96 EN/DE caption/layout cases and the
new28 name/query keyboard/logical paste cases. The latter cover complete-size
boundaries, reversed selection, coqueued rejection, transitional dot drafts and
retained undo/redo. History is probed on the exact same field through a separate
public widget after its read-only bit is removed for that component probe. It is
not an extra Files user action, a normal barrier dismissal or a native-UI claim.

Each of the three final native tasks passes **326 checkpoints**,28 clipboard
observations and five expected font-startup refusals, with60 virtual output
captures. The real original Files/controller and private Wayland producer retain
native preedit/transaction/selection/undo behavior and actual create/rename. The
foreign five-file wl-clipboard runtime and independent libatspi client check:

- Rejected keyboard paste retains the selected `é` and reversed caret in `aébc`;
  the original error state and disabled confirmation remain in effect.
- Independent EditableText Paste returns rejection for an invalid whole proposal,
  while the original text/caret remain unchanged.
- The original query control-character rule refuses a tab-containing proposal
  without changing `alpha` or its reversed selection.
- Previous immutable-copy, Cut-before-delete, valid range undo, invalid/oversized
  proposal, late-after-Tab and close-while-source-stopped cases still pass.

All180 decompressed capture hashes are verified. Every three-output group has
nonuniform pixels on at least one output. The valid app maps to output1 after
the existing malformed-font startup probe briefly consumes slot0. Four lossless
output1 PNGs are retained; rejected name, assistive rejection and rejected query
were inspected. Retained text/selection, red outline and original error message
are visible. This is not physical display, three-output migration, spoken/human
assistive or new child-document GUI acceptance. Original fixture bytes remain
intact, with only the ordinary task's final empty `xy` creation/rename result.

## Source and negative evidence

Compiled snapshot23 contains the final generator/C/CMake sources; native
snapshot24 differs only in the clipboard task and independently compiled peer.
All21 non-README example input files bind to the consumer commit; README is
the sole post-execution documentation difference among22 example files. All three
generated application files reverse exactly to the original inputs. The1434 SDK
source files match exact raw LF Git blobs, not just a builder revision string.
[Source binding](files-paste-source-binding.json) and [local receipt](files-paste-local.json)
retain source, commands, C17 flags, binary hashes, maps and observations.
The input-bundle count does not mean every bundled legacy script was executed;
the recorded build and native command arrays identify this turn's actual runs.

The preceding accepted generated view is recovered byte-for-byte from its
verified archive and compiled with the **same new0.1.26 SDK and original model**.
The unmodified new component test exits1 on lost reversed selection. A separate
counter-only copy bypasses just that selection assertion to expose the independent
history loss; it also exits1. Candidate tests retain all assertions. The actual
preceding native host then fails at checkpoint96 on the independent `aébc` caret/
selection expectation; its24 partial captures and unchanged fixture files remain.
These controls detect the old app integration, not an artificially failing SDK.

An actual old0.1.25 installed package is refused by the upgraded Files CMake
requirement. The first package probe ran outside the holder's TMPDIR environment
and failed compiler preflight on `/tmp` EDQUOT; it is not counted as package
rejection evidence. A fresh probe in the existing owned temporary environment
reaches and confirms the intended version mismatch. An initial component build
also failed `-Wmisleading-indentation`; only the test statement formatting was
corrected, with no warning/timeout/assertion suppression.

## Adoption and remaining work

[Example instructions](../../examples/35_aetheri_files_consumer/README.md) select
the new exact SDK independently of the unchanged default original consumer.
Aetheri can review/adopt this bounded Files paste integration separately; normal
app admission and policy review remain its own gates. Read-only Git survey finds
Aetheri `61510be94e31ce90ecdf8ccc1fbfcdfaf531c4f9`, still with reviewed SDK
`97a42647480bb05167cee5e4ae08cedd9147d80d`/0.1.23. That independently advancing
checkout was not built or modified. Previous exact main0a8d5e0 has
[seven required CI successes and two optional skips](paste-validation-ci.json);
those jobs do not execute this optional external Files consumer.

Open: full clipboard budgets/pressure/deadlines/owner loss and transport-error
feedback, broader editing/rejection-policy and font/locale/provider acceptance,
normal migration, remaining packet6 compositions, packets7-8 and final physical/
human/performance/adoption/three-output gates. All103 rows and catalogue SHA-256
`9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65` persist.
No1.0 release is declared.

[Archive receipt](files-paste-archive.json) and [cross-checks](files-paste-checks.json)
bind final and supporting artifacts. Obsolete owned build/source copies and then
completed SDK build copies were removed only after full archived-byte/mode/link
verification plus path/UID/process guards. Installed prefixes, original snapshots
and final consumer outputs remain available; no global quota/cache setting changed.

Final archive: `D:/NexaUI/out/nexaui-files-paste-20261108.tar.gz`,69,965,895
bytes, SHA-256 `4afe31983fe92863b9bc196667180cba0947fc8acd58a93a6dc35b32eabdda54`:
6480 files,59 links and3079 directories. All file bytes/sizes/modes, directory
modes and link targets are verified; listed inactive test FIFOs are not serialized.
The54,895,466-byte SDK support archive has SHA-256
`54cfa23d4c37d1d9ae2f0ab4e6637d293b3f23a4f59c1c875aadd5c64a7ca94a`
and preserves the retired three SDK build trees. The cross-check also matches
all65 explicit artifacts,1434 SDK sources,104 original app/producer inputs and
five foreign tool/runtime files, while keeping all103 requirement rows unchanged.
