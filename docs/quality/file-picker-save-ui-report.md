# Ordinary Save dialog and explicit existing-file review — 2026-11-07

Consumer source: `5b1b9b5ae0a81ece298426775d8818a77fc9a012`.
Installed NexaUI remains `ac0fb3b3b3aa53b2a4de927ec52a06deb33d5c36`, version
0.1.23. The eleven original application inputs remain Aetheri
`27de5cb228da0f37264fad64d964ad7a660d210d`. This connects the
[Save destination contract](file-picker-save-target-report.md) to the existing
[picker/owner](../../examples/32_aetheri_file_picker/README.md), advancing scoped
D07/L10/L11/I01/I07/T04/A02/A06/P05/Q02/Q04 evidence within packet 6.

## Behavior and application boundary

The owner offers Save as; mode 3 of `fp_owner_open` creates a fresh Save view.
`FP_SAVE=1` also builds/runs the standalone entry point. Its separate native
acceptance is not claimed by the owner workflows below. Existing public text
fields, clipboard/IME adapters, actions, scrolling, focus and nested modals are
reused. No public SDK API/ABI, version, external package, live Aetheri source,
consumer pin or 1.0.0 change is introduced.

The filename field holds literal UTF-8 draft bytes. The original context-free
name validator classifies them for the UI and the worker repeats that validation.
All mutable Files model calls and borrowed views remain exclusively worker-owned.
Enter or the fixed Save action submits only a complete valid name. An active
native preedit blocks both the button and Enter submission until commit/cancel.
No prefix, normalization or escaped display label is substituted for a name.

Selecting a regular-file row binds its original ordinal and clears the editor.
The complete original escaped label is shown separately. A nonempty typed name
or Clear existing selection removes that binding; explicit page/catalog changes
cannot retarget it. Enter on directories still navigates. A Save lookup retains
the same observed catalog/page/binding for return from review.

A NEW target can return to the owner. An EXISTING target opens a nested modal
with its complete escaped filename and held ancestor labels. Replace this file
requires explicit activation. Back receives default focus. Tab/Shift-Tab stays
inside that review, including its existing Slider-role scrollbar when present.
Fixed actions remain available while PageUp/PageDown reads the complete body.
Back/Escape releases the candidate descriptors, preserves the draft and restores
filename editing. A subsequent Escape cancels the chooser and restores its owner
trigger/note/last accepted result. A very small view retains the candidate but
offers no confirmation; Back there restores a valid editor focus after enlargement.
The resize grip belongs to the active review scope.

Ready destinations reject another worker request until transferred or released.
The owner requires explicit review approval for EXISTING, takes the destination
once, stores raw leaf separately from label, and closes the previous result only
on successful replacement. Cancel and retired-view completions cannot adopt it.
GUI teardown precedes owner-result release and original worker disposal as before.

The visible outcome is **Save destination selected**, never a completed write.
Actual create-only/overwrite admission, witness recheck, document state, export
binding, conflict/unknown/receipt and durable authority remain application-owned.
The preceding original-controller tests establish selected ordinary-user handoffs;
these UI tests deliberately assert that selection changes no filesystem content
or identity. No widget or native selection grants write access or proves success.

## Executed final checks

Fedora 44 VM, Linux 6.19.10, GCC 16.2.1, CMake 4.3, Python 3.14.3, real
Liberation Sans and frozen original `d56d695` headless compositor with three
virtual outputs. Static Release, shared Release and static Debug ASan/UBSan with
active leak detection each pass **6/6 CTests**, no skips/suppressions. Host and
model code retain strict conversion/shadow warnings as errors. Final receipts:
`evidence/commands-input11.json`, `{release,shared,asan}-input11.xml`.

The new real-font Save UI suite exercises actual original worker/filesystem
results, typed validation and Enter, once-only new/existing owner adoption,
refusal before explicit approval, default Back/background blocking, Back versus
Cancel, full 1,020-byte escaped-name reading, literal edits, candidate release,
64x64 compact/restore focus, cancellation while reviewing, a late retired Save
reply while a fresh view reads, retained note/results and final descriptor closure.
The five earlier worker/Save-foundation/view/owner/multi suites remain executed.

All **five final native workflows** pass with host/peer exit 0, no forced cleanup
or sanitizer diagnostics:

| Scope | Cases | Actual coverage |
| --- | ---: | --- |
| Save owner | 3 | Release desktop; maximum-entry shared German/200%-text/360x300/high contrast; sanitizer light/360x300 with controlled filename IME |
| Existing owner | 1 | Release/small complete selected-name reading and file/folder/Cancel behavior |
| Existing multi-owner | 1 | Sanitizer maximum-entry German/200%-text/small/high contrast |

The independent libatspi Save peer has no NexaUI linkage. Eleven stages use
actual Wayland Enter/Escape/End, an ASCII filename key, Tab/Shift-Tab, editable
native text and controlled preedit/commit. Enter during preedit must not submit;
the exact committed result is `freshxé`. Ordinary no-IME cases return `freshx`.
One full existing raw 255-byte name is selected, reviewed and confirmed; later
Back/Cancel retains it without replay. Native references to replaced contexts
refuse edits/actions. Independent `/proc/<host>/fd` observations verify the held
parent, actual raw-file witness, preceding parent release and retained result.
Every original fixture file's bytes/device/inode remain unchanged.

The small German review reads its complete escaped label in **24 overlapping
steps each direction**, keeping Back focused and the final label edge in the
lower half of the viewport. Desktop review is already fully visible and uses
zero steps, with explicit full-bound checks. Tab has one stop each direction on
desktop and two when the scrollbar exists. Final field/top/end captures were
retained; the filename-field image was also inspected at
`D:/NexaUI/out/picker-save-ui-20261107/final-filename.png`; `D:/NexaUI/out/picker-save-ui-20261107/final-review-bottom.png` was inspected
and shows the actual name ending plus fixed Replace/Back and safe focus.

Three deliberate source variants build and produce **two core and two native
targeted failures**: bypass existing-target approval; admit a filename during
preedit; retain retired review focus across compact Back/restore. The IME variant
also passes the ordinary no-IME native workflow, preserving that scope control.
`evidence/ui-mutants-input11.json` retains all eleven expected command exits.

## Retained earlier outcomes and limits

Input1 failed strict compilation on a shadowed local preedit variable, corrected
without changing warning flags. Earlier reading fixtures assumed a fixed number
of steps or that the desktop label must exceed its viewport; final tests derive
the range from actual geometry and separately prove already-visible cases.
An intermediate endpoint capture overscrolled beyond the filename; the final
endpoint requires the name ending actually visible. A first Tab check assumed
only two buttons, and its follow-up used ScrollBar rather than the existing
Slider role. Final checks include that actual published focus stop; no SDK role,
focus behavior, timeout or assertion was removed to obtain a pass.

One earlier sanitizer maximum multi regression timed out after partial selected-
name observation. It remains recorded, not declared fixed. Subsequent unchanged-
deadline runs of the published parent and new host both pass, as does the final
current regression. These do not establish the timeout's cause or a latency
budget. Four comparison starts hit EDQUOT before producing native result receipts;
their partial data/empty command files remain. Only independently byte/mode/link-
verified frozen duplicates were retired: 76,602 old Save-case nodes, six older
multi captures (84,326,502 bytes), then current intermediate captures after local
archive verification. No quota, reserve, clock, driver or other global setting
was changed; owned extracted source mtimes were normalized for builds.

The large multi regression retains known Cache.GetItems budget warnings; lazy
individual Accessible/Selection queries pass. The earlier unresolved AT-SPI heap
incident remains open. Neither native success nor an inspected fixture image is
human screenreader, installed-IME, physical-device or performance acceptance.

## Frozen evidence and remaining adoption

`D:/NexaUI/out/nexaui-picker-save-ui-20261107.tar.gz`:
SHA-256 `fbe20327f551bf2694c4eba4beba2d2a583e884c208c8170997bfd2082a4f64f`,
48,137,372 bytes, 198,283 regular files, 132 links and 3,974 directories plus
`archive-manifest.json`. Guest copy:
`/tmp/nexaui-picker-save-ui-20261107/nexaui-picker-save-ui-20261107.tar.gz`.
Local full manifest verification passed after transfer for every regular file,
directory and link.

The bundle binds all tested consumer inputs to the exact committed source,
allowing only line endings, checks the eleven original inputs and retains three
SDK prefixes, the full original source archive, builds/corpora/commands/logs,
actual peers/runtime/font inputs, counterexamples and preliminary outcomes.
Intermediate PPMs are retained in separately manifest-verified nested capture
archives; special fixtures are represented by path/type/mode metadata. Regular
bytes/sizes/modes, directories and link targets are independently verified after
transfer. This is artifact evidence, not a new process-RSS/energy acceptance.

Aetheri can independently evaluate this ordinary-user owner destination UI while
keeping its existing app/backend and SDK pin. Actual app write/progress/unknown
integration, file-type policy, standalone native acceptance, child parenting/
active OS close, active-IME resize/scale/seat loss, broader service/permission
workflows, other platforms and physical/human/performance gates remain required.
The rest of Files/device/software/component/trusted-system packet 6 and packets
7–8 remain active across the full 103-ID catalogue.

Subsequent CI observation: published documentation source
`fcb1ce3fef3f5f0dfe49321ae4920e125fd968b4` completes
[run 42375426418](https://github.com/Lulus792/NexaUI/actions/runs/42375426418)
with all seven required jobs successful (six platform/linkage variants and Linux
sanitizers). Optional Windows-ASan/fuzz jobs are skipped. This root SDK matrix
does not execute the separately pinned original-Aetheri optional examples and
does not expand the local native Save acceptance scope above.
