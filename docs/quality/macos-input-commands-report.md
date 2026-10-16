# Native editor commands — 2026-10-16

The initial AppKit editor-command path separates physical key state from native
text interpretation. General text fields also share visual navigation across
semantic commands and ordinary keys on Windows, Linux and macOS. This advances
M04/T16/T17/U05; those tasks and the F2/F4 gates remain incomplete.

## Public contract

`NX_EVENT_KEY_DOWN_CONSUMED` updates physical held/pressed state but cannot cause
widget shortcuts, plot/property actions or focus traversal. It denotes a press
already delegated to the native editor interpreter. Custom shortcut code must
inspect the event type instead of treating `nx_key_pressed` as an available
action. Releases still update physical state normally.

`NX_EVENT_TEXT_COMMAND` carries a fixed-width `text_command` payload. The
independent `NX_TEXT_COMMAND_EXTEND_SELECTION` bit is valid for movement/page
commands only. Existing event enum values, event size and payload/IME identity
offsets are retained; C17 compile-time assertions check the old/new layout.
Unknown command IDs, invalid flags and incomplete target/revision pairs are
rejected by the context queue. AppKit commands currently use the focused editor
without a revision tag; this does not add document reconversion support.

The standalone model handles logical grapheme/word, hard-line/paragraph and
document movement, deletion, select-all, newline and undo/redo. Physical modifiers
do not reinterpret a semantic command. Layout-dependent movement, scrolling,
clipboard and focus commands remain unhandled in that model. Numeric newline
commands commit a valid numeric draft; the rendered numeric widget still lacks
the full general-field native input path.

General fields use the same layout and password mask for navigation and painting.
They retain the selected visual caret affinity at bidi and soft-wrap boundaries;
a byte offset alone cannot identify both positions. Visual arrows, word and line
edges, vertical preferred-x movement and page movement use that geometry. Copy
preserves the visual caret, including the ordinary Ctrl+C path. Deleting a visual
range records the original selection directly in the atomic undo edit.

`PAGE_UP/DOWN` without selection extension scrolls the viewport and leaves the
source selection unchanged. Their selection variants scroll and move the caret.
`MOVE_PAGE_UP/DOWN` explicitly moves the caret; ordinary Windows/Linux Page keys
use this policy. Scroll-line/document commands do not move the selection. A
manually scrolled viewport does not immediately reveal the caret again during
painting. The renderer clips an offscreen caret. Clipboard actions use existing
optional callbacks; password copy/cut remains suppressed.

## AppKit ownership

For active editors, physical `keyDown:` passes editor keys to
`interpretKeyEvents:`. AppKit then supplies committed Unicode or selectors through
`doCommandBySelector:`. The bounded selector table covers logical/visual and word
movement, line/paragraph/document edges, deletion, selection, scrolling, newline,
tab/backtab, cancel, clipboard and undo/redo. Command+A/C/X/V/Z/Shift+Z enter the
same action path. Original Command/Control/Option identity is preserved. Unrelated
Command shortcuts such as Quit remain available to the host.

Supported ordinary commands first commit pending marked text. Cancel or Undo
discards pending composition. An unsupported text selector is ignored locally;
it must not be sent to `super` or the next responder. Apple's specialized
[NSTextInputClient method contract](https://developer.apple.com/documentation/appkit/nstextinputclient/docommand%28by%3A%29)
explicitly requires this, whereas the archived general
[NSResponder key-handling discussion](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/EventOverview/HandlingKeyEvents/HandlingKeyEvents.html)
describes forwarding in its broader context. The first implementation followed
that general behavior; review of the specialized contract corrected the code
and the native responder-sink assertion before final acceptance.

The [standard binding protocol](https://developer.apple.com/documentation/appkit/nsstandardkeybindingresponding)
distinguishes page/scroll operations and selection variants. Apple's
[keyboard guidance](https://support.apple.com/en-la/102650) also distinguishes
document scrolling from caret navigation. These are the basis for the current
subset, not a claim of complete support for every application or custom binding.

## Executed regressions

`nexaui_macos_window` creates actual hidden AppKit windows. The command tests:

- Deliver a consumed arrow through the real production `keyDown:` into a test
  interpreter and prove the public text model does not edit or navigate twice.
- Invoke word-selection and Command+A paths and verify action flags and original
  modifiers. Check that Command+Q is not claimed as an editor equivalent.
- Attach a responder sink and prove an unsupported selector never reaches it.
- Invoke the actual AppKit interpreter with Control+E, apply its returned event
  through the public text model and observe the first line's end. Apply native
  word deletion and verify the resulting bytes.
- Cancel marked text without a text/command commit, disable input and deliver a
  late selector after native owner teardown without accessing freed C storage.

Common tests cover command validation/event layout, consumed Tab versus semantic
Tab focus, logical model edits and undo, numeric commit, consumed property arrows,
RTL and mixed-direction visual caret positions, actual soft wrapping, selection,
visual deletion/undo, page scrolling versus caret movement, headless caret pixels,
clipboard and masked grapheme/source offsets. Password copy/cut does not write
the clipboard.

Adversarial checks were compiled and executed with the same real fixtures:

| Deliberately older or incorrect implementation | Observed failure |
| --- | --- |
| Original `0cc0c3c` text model with the new public test | First semantic movement is unhandled |
| Original physical-key reads restored in property widgets | Consumed Right changes the number |
| Cached visual affinity removed from painting | Mixed-bidi caret positions stop progressing monotonically |
| `ddad479` core with the newer field test | Ordinary Right in RTL uses the wrong source position |
| Scroll-only page branch removed from a current core copy | Page Down changes the selection |
| `c747d93` with the copy regression added | Copy moves the soft-wrap caret from its chosen line |

The mutation copies and raw logs are in ignored local `out/` directories; they
are not installed product sources. Their failures establish these specific test
sensitivities, not exhaustive negative coverage. Unbreakable digit strings and
recorded off-clip commands were not treated as wrapping/visibility evidence;
the final tests use actual Unicode break opportunities and rendered pixels.

Local MSVC Debug static/shared builds passed seven selected input/editing tests
before the copy follow-up. Linux Clang 22.1.3 ASan/UBSan/LeakSanitizer also passed
those seven. After the copy correction, Windows static passed the seven selected
tests again and the Linux sanitized text-field test passed with leak detection
enabled. Full desktop CI below covers the final source in both library variants.

The final logical-model [libFuzzer receipt](text-command-fuzz.json) records 50,000
executions, seed 181017, maximum input length 512, eleven initial seeds, 1,155
coverage counters and exit 0 in 99.26 seconds. ASan/UBSan reported no finding.
Leak detection is disabled only for that libFuzzer driver because of the
separately documented [Alpine driver allocation](text-fuzz-report.md); the targeted CTests use
`detect_leaks=1`. The later copy change is in widget presentation, outside this
logical-model fuzz target. Fuzzing does not exercise AppKit or visual layout.

## Native CI

The exact source/run/job identities, executed command tests, compiler and skips
are retained in [the CI receipt](macos-input-commands-ci.json). Final source
`03bb8cfda5e05249d561ef317b2097020b021087`, including the copy-affinity correction,
passes all seven required jobs in [run 38948530828](https://github.com/Lulus792/NexaUI/actions/runs/38948530828):

| Configuration | Passed | Skipped |
| --- | ---: | ---: |
| Windows static / shared | 99 / 99 | 0 / 0 |
| macOS static / shared | 86 / 86 | 0 / 0 |
| Ubuntu static / shared | 100 / 100 | 7 / 7 |
| Linux ASan/UBSan | 100 | 7 |

The earlier corrected command/scroll implementation at `c747d93` also passed
all seven required jobs in run 38945532031. The final receipt refers to the
newer source, not that historical run. Ubuntu skips are explicit unavailable
native Wayland display cases.

Both macOS variants execute the AppKit window test, common editing tests and
independent OS AX client. These are native SDK/runtime tests, not a Linux build
with an Apple preprocessor branch. Optional Windows ASan and CI fuzz jobs are
skipped by workflow policy; the local sanitizer/fuzz runs are separate evidence.

## Remaining acceptance

Physical keyboard layouts, installed non-Latin IMEs, custom key-binding files,
full document/surrounding replacement and revision-bound reconversion remain
open. The initial AppKit replacement contract still accepts only default/full
marked-text replacement. OS secure keyboard capture, full numeric-widget native
integration, complete text/range accessibility and real VoiceOver/Narrator/Orca
tasks are not established. CI runners and software rendering do not establish
physical display, hardware or release readiness on every operating system.
