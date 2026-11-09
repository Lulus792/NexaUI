# Original Editor: paint after initial modal focus

Consumer/test source: **`b0a254c80102d0458893d997ce5dc0fef7f1bc2a`**.
Installed SDK: **0.1.33 / `1e00a6f4f3a0ab3a7da1aa35a36f02679421366b`**.
Original Editor: **`7bbe7042ecd48582b11ef7053a4dbeade6b02351`**.

This closes a specific cause of the focus/caret differences retained in the
[previous native follow-up](editor-native-followup-report.md). The separate
generated consumer now schedules a followup frame when initial review/Open/Save
focus changes after the controls have already painted. It uses `nx_focus_set`
and the existing event/dirty/presenter contracts. SDK production code, API/ABI,
version and dependencies are unchanged. This is a partial native checkpoint:
two ordinary native profiles pass, while Release and attributes-OFF fail in
the unchanged one-second clipboard/assistive workflow. Those failures remain
open and are preserved separately from the focus regression.

The [exact-source CI receipt](editor-focus-paint-ci.json) binds
[run42716469496](https://github.com/Lulus792/NexaUI/actions/runs/42716469496),
attempt1: all seven required jobs pass on Linux, Windows and macOS, with no
CTest skips. Two optional jobs are skipped. The portable observer exercises
the completed-focus capture predicate; the full original Editor owner remains
the separate Linux local/native evidence described below. The
[raw archive receipt](editor-focus-paint-archive.json) binds all retained
successes, failures and diagnostics and names the unchanged prerequisite archive.

## Cause and correction

The original buttons capture `interaction.focused` before painting. The review
and Open picker set their initial button focus only after those buttons have
painted; Save sets the filename-field focus after its widget has painted. The
frame then overwrote its dirty flag with the presenter-BUSY result. A READY
frame could therefore publish focused semantics without painting that focus
or scheduling the next frame. Later input or a presenter retry could conceal
the omission.

The checked generator marks only these three late-focus sites. A successful
change of focus retains one dirty followup; each frame resets the marker.
The native-document variant retains this marker together with its existing
accepted-edit/header followup. Native event draining and request routing are
unchanged. Once the focused controls have painted, no continuous redraw is
introduced. Diagnostics expose `focus_pending`; optional `--focus-paint` waits
for a latest READY diagnostic with zero pending focus work before each original
capture. Existing capture delay, events, deadlines and assertions remain.

## Actual component and source evidence

The [local receipt](editor-focus-paint-local.json) and
[source binding](editor-focus-paint-source-binding.json) verify 110 exact Git
inputs, 133 actual C17 owner/independent-peer objects and 28 regenerated C files.
Four local configurations pass 23/23/23/21 CTests; the exact-source root helper
adds one, for **91 executed tests**. Static/shared Release, Debug-O1
ASan/UBSan/LSan and attributes-OFF remain distinct configurations.

`editor.focus_paint` compiles the actual generated owner and runs its complete
frame, real widgets, semantics, fonts and headless rasterizer. Only native
presentation and semantic publication are replaced at the OS boundary. Each
review/Open/Save case runs with initial READY and initial BUSY, verifies semantic
focus and actual focus-colored pixels, requires a followup and stable idle, and
compares the complete settled image with another frame. These are **24 passing
modal cases** across the four configurations, not native presentation claims.

The identical test source rejects the actual old generator in Release and
ASan/UBSan/LSan: all three initial-READY cases schedule no followup and contain
zero focus-colored pixels in the control. A manually forced second frame adds
888 review, 672 Open and 1,516 Save focus pixels. The 888 review pixels match
the previously captured missing outline. The negative sanitizer run exits1
after cleanup without ASan/UBSan/LSan diagnostics.

At read-only comparison revision `9fb456781de2c8ab73158f762ca74be87224cb1d`,
52 of the 53 consumed original inputs are unchanged. The sole changed input,
`spec/nexaui-inputs.json`, adds `model_support` entries; consumed Editor and
support source bytes remain unchanged. That current checkout is not a newly
tested consumer revision. No Aetheri checkout, pin or build was modified here.

## Native results and retained failures

Shared and ASan/UBSan/LSan each complete the original native model, keyboard,
IME, independent AT-SPI, clipboard, held-EOF, document-presentation/attributes
and repeated-close tasks: **168 complete captures and 52 forced pre-EOF frames**.
Their 28 native paste replies are actual replies within the unchanged one-second
client deadline; no transport timeout is accepted in these two positive receipts.
The private original compositor, real foreign clipboard children, original
12-byte payload/partial UTF-8 and writer-pipe observations remain in use.

The [whole-image comparison](editor-focus-paint-images.json) checks all 84
Shared/ASan pairs with no masks or tolerance: **82 equal, two different**.
The previous review outline and filename-field caret/border differences are
absent in this pair. Both remaining differences are 1,011 pixels in the displayed
per-run temporary directory name. Six complete review/Save images were inspected;
the review Cancel outline and Save filename caret/border are visible in both
profiles. This is limited to these completed software-output tasks.

Release exits1 during a held-EOF preedit case. The saved early-reply observation
finds the real writer pipe still open and all12 bytes written. Actual peer logs
identify libatspi timeouts at 1,001,320/1,001,382us, not successful pre-EOF paste.
Attributes-OFF also exits1 in the held-EOF workflow. These are failed ordinary
runs, not four-profile native acceptance. The current peer can clear a transport
error in a refused-paste phase; the positive receipts independently demand
`error=none`. That peer check still needs hardening.

A separately labeled diagnostic Release run adds Python cProfile and an
independent private-bus monitor. It completes, but cannot replace either failed
ordinary run or prove a repair. It records 2,366 full log calls taking 27.634s
within a 92.595s profile; `stream_frames` accounts for 5.027s over13 calls.
Repeated full-log reads are a concrete next investigation target, not a proved
cause of every timeout or an application-performance result. A preceding
diagnostic launch failed before application startup because the private runpy
wrapper omitted the script import path; that failure and its corrected wrapper
remain separate. No deadline or assertion was relaxed and no SDK repair for
the timeout is claimed.

## Remaining work

Investigate and bound controller observation costs with exact full-log-content
counterexamples, harden refused-paste error checking, then rerun the failed native
profiles with the original events, one-second deadline and EOF/CPU/frame guards.
Retain these failures and the diagnostic regardless of later results.

Actual selected Aetheri-compositor acceptance, installed external IMEs and
screenreaders, broader UIA/AX/font interfaces, physical/human/design/performance
and normal adoption remain open. All103 requirements and remaining packets6–8
retain their scope/status; this increment does not declare 1.0 or goal completion.
