# Original Files native text clipboard — 2026-11-08

Implementation: `da18553a912cd1972cbcf536491ab60b3cc2d359`.
The separately generated Files variant now supplies the existing public native
clipboard adapter to its text fields and routes logical Copy/Cut/Paste alongside
Undo/Redo. There is no second clipboard or Files model implementation. Default
OFF still builds the unchanged original 0.1.23 consumer; the generated variant
uses the independently installed 0.1.25 SDK. No SDK source/API/ABI/version, active
Aetheri source/build or reviewed pin changes are part of this checkpoint.

Scoped catalogue evidence: B01/B03/B04, I01/I02/I05/I07, T01/T03/T06/T11,
A02/A04, P04/P05, Q02/Q03/Q06/Q08. All 103 IDs and remaining packets 6–8 stay
active; this is no complete Clipboard, Files, desktop or final adoption gate.

## Retained contracts

Copy takes an immutable native snapshot; Cut changes the selection only after
successful copy. Native keyboard and accessible paste reuse the installed SDK's
asynchronous reads and focus, field identity/revision, selection and close checks.
Clipboard data never becomes a command. File Cut/Paste outside an editor remains
the original app-owned file operation, separate from this text integration.

The existing single-line widget converts CR/LF to spaces. The original Files
validator then judges the complete proposed draft; invalid filenames and
oversized values retain the previous app text, mark it invalid and block
confirmation until explicit clear. Original rollback uses `nx_text_field_set_text`
and resets field history. This checkpoint does not preserve prior field history
on rejection. Valid paste replacement and Cut remain single undoable edits.

Keyboard paste retains the SDK's 32 MiB/five-second transport budget, while the
accepted Files draft is at most 255 bytes. The tests below reach 65,537 bytes,
not the entire transport budget. Native AT-SPI paste has its separate bridge
limits. EOF alone cannot prove that an external source sent its entire value.
Native writes also require an available Wayland input serial; neither an
accessible request nor a widget grants compositor/session authority.

## Exact inputs and executed profiles

[Source binding](files-clipboard-source-binding.json),
[native/build observations](files-clipboard-local.json) and
[final checks](files-clipboard-checks.json) retain exact commands and artifacts.

The immutable original app is Aetheri
`7483286bcaafbeaced5831f72cb4d0e0cad9a158` (87 checked inputs). The separate
private producer remains `d56d69523c33a48648af343e7d36526f84518c90` (17 inputs),
with its previously checked two-file reversible delta and exact binary. The
installed SDK remains `ca1cb3db26badde2c6fbe3cc1bae1de1b149ad18` / 0.1.25;
its source/package provenance is retained in the preceding
[atomic transaction binding](files-ime-atomic-source-binding.json).

Input13 built shared Release, static Release and static ASan/UBSan/LSan consumers.
Each passed **24/24 CTests**, including all ten controller cases, with **48
explicit C17/Werror objects** per profile. Input16 differs only in README and
three native-test files: `clipboard_task.py`, `ime_atspi.c`, `run_ime.py`. Every
other input, including all compiled app/generator inputs, is identical. All
16 executed build/runtime/test inputs bind to the implementation Git revision;
the README was clarified afterward without changing executable input.

After archive-verified space reclamation, fourteen selected static native
binary/generated-source files were recovered byte-for-byte from the verified
input13 artifact. They were not rebuilt, relabeled as fresh builds or replaced
with weaker sanitizer options. All three native tasks use those exact compiled
artifacts; native test peers are independently compiled from input16.

Each final profile passes **290 native checkpoints**, retains **19 clipboard
observations** and **51 virtual captures**, and exits through the app with code 0
and `failed=0`. Existing IME/200%-text/whole-proposal/create/rename checks remain
in that same task. Clipboard additions verify:

- Foreign `wl-copy` owns the synthetic Unicode value `Grüße 🌍 é`; independent
  `wl-paste` reads its exact bytes and native paste reaches the actual Name field.
- Native Copy replaces the foreign owner: an actual native `set_selection`
  request and the previous owner's normal cancellation precede byte comparison.
  Clearing the app draft does not change the copied snapshot. Cut, paste and
  whole-selection undo/redo follow the same actual field/model path.
- A separate libatspi client performs CopyText, PasteText and CutText with actual
  model effects. A fresh harmless native Shift key supplies the selection serial
  before Cut after the foreign receiver's temporary focus popup.
- A middle selection containing `é` is replaced with `Z`; one Undo restores
  `aébc` and its scalar selection, and Redo restores `aZbc` at the insertion caret.
- Invalid `bad/name`, 256-byte and 65,537-byte values retain `s`; the independent
  assistive peer observes disabled confirmation and Enter creates no prefix file.
  `name\nline` follows the existing documented single-line space conversion.
- A stopped foreign producer leaves paste pending. Tab retires the target before
  the producer resumes. A source-send plus parent-observed return to Wayland
  socket wait precede checking that the old draft stayed unchanged.
- Close ends the app while a second producer remains stopped. Its actual stopped
  state is checked again after app exit 0, before cleanup resumes that producer.

The original `alpha`/`é` fixture bytes stay intact; only the ordinary existing
create/rename task produces the final empty `xy`. Actual shared mappings select
the checked SDK; Files mappings exclude SDL, X11, EGL and Vulkan. Foreign tools
link no NexaUI/model code. All five tool/runtime files match the previously
accepted independent wl-clipboard 2.2.1 artifact, not just their version output.

The same task against the preceding accepted Files IME binary without clipboard
binding exits 1 at the independent Unicode text/caret check. Its fixture files
remain unchanged. That compiled before/after control is separate from transient
test-environment/setup failures.

Four lossless PNGs were converted from the shared captures; three were inspected
(Unicode paste, range replacement, rejected oversized proposal). The retained
text, caret and invalid outline are visible. The existing missing globe glyph
and shortened Cancel label remain concrete font/layout work. These are virtual
software captures, not physical display or human screenreader acceptance.

## Preserved failures and open continuation

The first native pilot failed during capture before reaching Clipboard; an owned
write probe reproduced `EDQUOT` after 56,213,504 bytes. Exact archived obsolete
build copies were verified before removal; original failed captures/logs remain.
No global quota/cache setting or active Aetheri tree was changed.

A first close fixture attempted Ctrl+N while the preceding IME task deliberately
left active preedit. The correction waits for actual native disable/reactivation
after Escape. Initial native Cut was unavailable after a foreign focus popup;
fresh native input makes it succeed. A tested frame-deferral hypothesis did not
fix that condition and is absent from the implementation. The initial negative
receipt checker also used the wrong argument slice; its failure is retained and
only the negative task was repeated with explicit exit-code recording. No native
assertion, deadline or accepted binary was weakened.

A read-only Git survey of Aetheri
`aca8235dedc5c2b9c05eb059f8a66d8d225b8d9d` still finds reviewed SDK pin
`97a42647480bb05167cee5e4ae08cedd9147d80d`. This is no runtime qualification
of that independently evolving checkout. Previous exact main `13542f3` has
[seven required CI successes and two optional skips](files-ime-atomic-ci.json);
those jobs do not execute this optional external consumer.

Open: clipboard pressure/full-budget/deadline/owner-loss cases, rejection-history
policy, real IME/provider acceptance, font fallback/localization/layout polish,
normal admission/migration, remaining packet 6 compositions, packets 7–8 and all
final physical/human/performance/three-output gates. Catalogue SHA-256 remains
`9c932872caeba8007bf05b86f7c167b493260709b23c54d709222a5f19539e65`.
[Artifact receipt](files-clipboard-archive.json) lists final/supporting archives;
their file bytes/sizes/modes, directory modes and link targets are checked.

Final host artifact: `D:/NexaUI/out/nexaui-files-clipboard-20261108.tar.gz`,
46,538,032 bytes, SHA-256
`11f4f73f0f3231d0de6d5cf556e1a04e40d53db86a4a80de5f9d2bf4b2deeac9`:
5041 files, 54 links and 2580 directories. Listed inactive test FIFOs are not
serialized. All 48 explicit final artifact rows, the native input snapshot,
five foreign tool/runtime files and 1397 frozen SDK source files also match its
verified manifest. The supporting input13 archive supplies retired compiler
objects/caches/raw tests. No final quality or normal adoption is inferred from
successful archive verification.
